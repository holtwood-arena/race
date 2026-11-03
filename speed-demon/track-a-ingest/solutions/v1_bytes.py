#!/usr/bin/env python3
"""Track A v1: byte-level parsing, single-threaded CPython.

vs reference: strptime -> fixed-offset integer math on raw bytes;
endpoint str -> int id (1,842 distinct); group key (hour,ep) -> one int.
"""

import argparse
import os
import sys

SPAN_DAYS = 4  # 2026-10-30 .. 2026-11-02, fixed by spec
HOURS = 72
Z48 = 48  # ord('0')


def main(data_dir: str, out_dir: str) -> None:
    with open(os.path.join(data_dir, "requests.log"), "rb") as f:
        buf = f.read()

    ep_map = {}            # endpoint bytes -> id
    ep_list = []           # id -> endpoint bytes
    groups = {}            # (hidx << 11) | epid -> [count, errors, latencies]

    index = buf.index
    get_g = groups.get
    get_e = ep_map.get

    i, n = 0, len(buf)
    while i < n:
        # ts: fixed 27 bytes, day at +8/+9, hour at +11/+12
        day = buf[i + 8] * 10 + buf[i + 9] - 528
        hh = buf[i + 11] * 10 + buf[i + 12] - 528
        hidx = ((day - 30) if day >= 30 else (day + 1)) * 24 + hh

        s1 = index(32, i + 28)          # end of method
        s2 = index(32, s1 + 1)          # end of endpoint
        j = index(10, s2 + 5)           # LF after latency

        ep = buf[s1 + 1:s2]
        epid = get_e(ep)
        if epid is None:
            epid = len(ep_list)
            ep_map[ep] = epid
            ep_list.append(ep)

        k = (hidx << 11) | epid
        g = get_g(k)
        if g is None:
            groups[k] = [1, 1 if buf[s2 + 1] == 53 else 0,
                         [int(buf[s2 + 5:j])]]
        else:
            g[0] += 1
            if buf[s2 + 1] == 53:
                g[1] += 1
            g[2].append(int(buf[s2 + 5:j]))
        i = j + 1

    # output: sort by hour, then endpoint bytes (epid order != byte order)
    by_hour = [[] for _ in range(SPAN_DAYS * 24)]
    for k in groups:
        by_hour[k >> 11].append(k & 2047)

    out = []
    ap = out.append
    for hidx in range(SPAN_DAYS * 24):
        eps = by_hour[hidx]
        eps.sort(key=ep_list.__getitem__)
        for epid in eps:
            cnt, errs, lat = groups[(hidx << 11) | epid]
            lat.sort()
            p50 = lat[(50 * cnt + 99) // 100 - 1]
            p95 = lat[(95 * cnt + 99) // 100 - 1]
            p99 = lat[(99 * cnt + 99) // 100 - 1]
            ppm = errs * 1000000 // cnt
            day = 30 + hidx // 24
            mon, d = (10, day) if day <= 31 else (11, day - 31)
            ap(f"2026-{mon:02d}-{d:02d}T{hidx % 24:02d}:00:00Z "
               f"{ep_list[epid].decode()} {cnt} {p50} {p95} {p99} "
               f"{errs} {ppm}\n")

    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "summary.txt"), "w") as f:
        f.writelines(out)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--data", default="/data")
    p.add_argument("--out", default="/out")
    a = p.parse_args()
    main(a.data, a.out)
