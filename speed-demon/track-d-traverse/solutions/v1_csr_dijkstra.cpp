// Track D v1: CSR + per-query Dijkstra (early exit) + query-level parallelism.
// Build: g++ -O3 -march=native -pthread -o v1 v1_csr_dijkstra.cpp
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

static const uint64_t INF = ~0ULL;

// ---- fast u64 parser over a byte range ----
struct Cur { const char* p; const char* end; };
static inline uint64_t num(Cur& c) {
    while (c.p < c.end && (*c.p < '0' || *c.p > '9')) ++c.p;
    uint64_t v = 0;
    while (c.p < c.end && *c.p >= '0' && *c.p <= '9')
        v = v * 10 + (*c.p++ - '0');
    return v;
}

static const char* mmap_ro(const char* path, size_t& sz) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); exit(1); }
    struct stat st; fstat(fd, &st); sz = st.st_size;
    const char* b = (const char*)mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    madvise((void*)b, sz, MADV_SEQUENTIAL);
    close(fd);
    return b;
}

// ---- Dijkstra per thread ----
struct Worker {
    uint32_t n;
    const uint32_t* off;   // n+1
    const uint32_t* head;  // m
    const uint32_t* wgt;   // m
    std::vector<uint64_t> dist;
    std::vector<uint32_t> heap;      // indices into hd
    std::vector<uint64_t> hd;        // heap (dist,node) packed as dist in high, node low? no: parallel arrays
    std::vector<uint32_t> hn;
    std::vector<uint32_t> touched;

    void init(uint32_t n_, const uint32_t* o, const uint32_t* h, const uint32_t* w) {
        n = n_; off = o; head = h; wgt = w;
        dist.assign(n, INF);
        hd.reserve(1 << 16); hn.reserve(1 << 16);
        touched.reserve(1 << 16);
    }
    void reset() { for (uint32_t u : touched) dist[u] = INF; touched.clear(); }

    inline void hpush(uint64_t d, uint32_t v) {
        size_t i = hd.size(); hd.push_back(d); hn.push_back(v);
        while (i) { size_t p = (i - 1) >> 1; if (hd[p] <= d) break;
            std::swap(hd[i], hd[p]); std::swap(hn[i], hn[p]); i = p; }
    }
    inline void hpop(uint64_t& d, uint32_t& v) {
        d = hd[0]; v = hn[0];
        uint64_t ld = hd.back(); uint32_t lv = hn.back();
        hd.pop_back(); hn.pop_back();
        if (hd.empty()) return;
        size_t i = 0, sz = hd.size();
        for (;;) { size_t l = 2*i+1, r = l+1, m = i;
            if (l < sz && hd[l] < ld) m = l;
            if (r < sz && hd[r] < (m == i ? ld : hd[m])) m = r;
            if (m == i) break;
            hd[i] = hd[m]; hn[i] = hn[m]; i = m; }
        if (!hd.empty()) { hd[i] = ld; hn[i] = lv; }
    }

    int64_t query(uint32_t s, uint32_t t) {
        if (s == t) return 0;
        reset();
        dist[s] = 0; touched.push_back(s);
        hd.clear(); hn.clear();
        hpush(0, s);
        while (!hd.empty()) {
            uint64_t d; uint32_t u;
            hpop(d, u);
            if (d != dist[u]) continue;          // stale
            if (u == t) { int64_t r = (int64_t)d; return r; }
            for (uint32_t e = off[u], ee = off[u+1]; e < ee; ++e) {
                uint64_t nd = d + wgt[e];
                uint32_t v = head[e];
                if (nd < dist[v]) {
                    if (dist[v] == INF) touched.push_back(v);
                    dist[v] = nd; hpush(nd, v);
                }
            }
        }
        return -1;
    }
};

int main(int argc, char** argv) {
    std::string data_dir = "/data", out_dir = "/out";
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--data")) data_dir = argv[i + 1];
        else if (!strcmp(argv[i], "--out")) out_dir = argv[i + 1];
    }
    size_t gsz, qsz;
    const char* gb = mmap_ro((data_dir + "/graph.txt").c_str(), gsz);
    const char* qb = mmap_ro((data_dir + "/queries.txt").c_str(), qsz);

    // ---- header + query parse ----
    Cur gc{gb, gb + gsz};
    uint32_t n = (uint32_t)num(gc);
    uint32_t m = (uint32_t)num(gc);
    Cur qc{qb, qb + qsz};
    std::vector<std::pair<uint32_t,uint32_t>> queries;
    while (qc.p < qc.end) {
        uint32_t s = (uint32_t)num(qc);
        if (qc.p >= qc.end) break;
        uint32_t t = (uint32_t)num(qc);
        queries.emplace_back(s, t);
    }

    const int T = 12;
    // ---- pass 1: per-chunk degree counts ----
    std::vector<std::vector<uint32_t>> deg(T, std::vector<uint32_t>(n, 0));
    std::vector<const char*> lo(T), hi(T);
    size_t chunk = (gc.p - gb);            // edges start here
    size_t span = gsz - chunk;
    {
        std::vector<std::thread> th;
        for (int t = 0; t < T; ++t) {
            const char* l = gb + chunk + t * span / T;
            const char* h = (t == T-1) ? gb + gsz : gb + chunk + (t+1) * span / T;
            if (t > 0) { const char* x = (const char*)memchr(l, '\n', gb+gsz-l); l = x ? x+1 : gb+gsz; }
            if (t < T-1) { const char* x = (const char*)memchr(h, '\n', gb+gsz-h); h = x ? x+1 : gb+gsz; }
            lo[t] = l; hi[t] = h;
        }
        for (int t = 0; t < T; ++t)
            th.emplace_back([&,t]{ Cur c{lo[t], hi[t]};
                while (c.p < c.end) {
                    if (*c.p < '0' || *c.p > '9') { ++c.p; continue; }  // trailing LF: no phantom edge
                    uint32_t u=(uint32_t)num(c); num(c); num(c); if (u<n) deg[t][u]++; }});
        for (auto& x : th) x.join();
    }
    // ---- offsets ----
    std::vector<uint32_t> off(n + 1);
    off[0] = 0;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t d = 0;
        for (int t = 0; t < T; ++t) d += deg[t][i];
        off[i+1] = off[i] + d;
    }
    deg.clear(); deg.shrink_to_fit();
    // ---- pass 2: fill CSR via shared atomic cursors ----
    // (same u appears in multiple chunks -> private cursors would collide)
    std::vector<uint32_t> head(m), wgt(m);
    {
        std::vector<std::atomic<uint32_t>> cur(n);
        for (uint32_t i = 0; i < n; ++i) cur[i].store(off[i], std::memory_order_relaxed);
        std::vector<std::thread> th;
        for (int t = 0; t < T; ++t)
            th.emplace_back([&,t]{
                Cur c{lo[t], hi[t]};
                while (c.p < c.end) {
                    if (*c.p < '0' || *c.p > '9') { ++c.p; continue; }  // trailing LF: no phantom edge
                    uint32_t u=(uint32_t)num(c), v=(uint32_t)num(c), w=(uint32_t)num(c);
                    uint32_t e = cur[u].fetch_add(1, std::memory_order_relaxed);
                    head[e] = v; wgt[e] = w;
                }
            });
        for (auto& x : th) x.join();
    }
    munmap((void*)gb, gsz);   // graph never revisited

    // ---- queries: parallel ----
    std::vector<int64_t> ans(queries.size());
    {
        std::vector<Worker> W(T);
        std::vector<std::thread> th;
        for (int t = 0; t < T; ++t)
            th.emplace_back([&,t]{
                W[t].init(n, off.data(), head.data(), wgt.data());
                for (size_t i = t; i < queries.size(); i += T)
                    ans[i] = W[t].query(queries[i].first, queries[i].second);
            });
        for (auto& x : th) x.join();
    }

    // ---- output ----
    std::string out; out.reserve(queries.size() * 12);
    char buf[24];
    for (int64_t d : ans) {
        char* o = buf;
        if (d < 0) { *o++ = '-'; *o++ = '1'; }
        else { char tmp[20]; int k = 0; uint64_t v = d;
            do { tmp[k++] = '0' + v % 10; v /= 10; } while (v);
            while (k) *o++ = tmp[--k]; }
        *o++ = '\n';
        out.append(buf, o - buf);
    }
    std::string outpath = out_dir + "/distances.txt";
    FILE* f = fopen(outpath.c_str(), "w");
    if (!f) { perror("fopen"); return 1; }
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return 0;
}
