// Track D v2: bidirectional Dijkstra per query (fwd+rev CSR, two heaps,
// meet-in-middle termination top_f + top_b >= mu). Query-level parallelism.
// Build: g++ -O3 -march=native -pthread -o v2 v2_bidir.cpp
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

struct Heap {
    std::vector<uint64_t> hd;
    std::vector<uint32_t> hn;
    void clear() { hd.clear(); hn.clear(); }
    bool empty() const { return hd.empty(); }
    uint64_t top() const { return hd.empty() ? INF : hd[0]; }
    void push(uint64_t d, uint32_t v) {
        size_t i = hd.size(); hd.push_back(d); hn.push_back(v);
        while (i) { size_t p = (i - 1) >> 1; if (hd[p] <= d) break;
            std::swap(hd[i], hd[p]); std::swap(hn[i], hn[p]); i = p; }
    }
    void pop(uint64_t& d, uint32_t& v) {
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
        hd[i] = ld; hn[i] = lv;
    }
};

struct Worker {
    uint32_t n;
    const uint32_t *offF, *headF, *wgtF;   // forward CSR
    const uint32_t *offB, *headB, *wgtB;   // reverse CSR
    std::vector<uint64_t> distF, distB;
    std::vector<uint32_t> tF, tB;          // touched lists
    Heap hF, hB;

    void init(uint32_t n_, const uint32_t* oF, const uint32_t* hF_, const uint32_t* wF,
              const uint32_t* oB, const uint32_t* hB_, const uint32_t* wB) {
        n = n_; offF = oF; headF = hF_; wgtF = wF;
        offB = oB; headB = hB_; wgtB = wB;
        distF.assign(n, INF); distB.assign(n, INF);
        tF.reserve(1 << 16); tB.reserve(1 << 16);
        hF.hd.reserve(1 << 16); hF.hn.reserve(1 << 16);
        hB.hd.reserve(1 << 16); hB.hn.reserve(1 << 16);
    }
    void reset() {
        for (uint32_t u : tF) distF[u] = INF; tF.clear();
        for (uint32_t u : tB) distB[u] = INF; tB.clear();
    }

    inline void expand(std::vector<uint64_t>& dist, std::vector<uint64_t>& other,
                       std::vector<uint32_t>& touch, Heap& h,
                       const uint32_t* off, const uint32_t* head, const uint32_t* wgt,
                       uint64_t& mu) {
        uint64_t d; uint32_t u;
        h.pop(d, u);
        if (d != dist[u]) return;                      // stale
        if (other[u] != INF) {                         // meet: settled this side,
            uint64_t c = d + other[u];                 // any dist on other side
            if (c < mu) mu = c;
        }
        for (uint32_t e = off[u], ee = off[u+1]; e < ee; ++e) {
            uint64_t nd = d + wgt[e];
            uint32_t v = head[e];
            if (nd < dist[v]) {
                if (dist[v] == INF) touch.push_back(v);
                dist[v] = nd; h.push(nd, v);
            }
        }
    }

    int64_t query(uint32_t s, uint32_t t) {
        if (s == t) return 0;
        reset();
        distF[s] = 0; tF.push_back(s); hF.push(0, s);
        distB[t] = 0; tB.push_back(t); hB.push(0, t);
        uint64_t mu = INF;
        while (!hF.empty() && !hB.empty()) {
            if (hF.top() + hB.top() >= mu) break;      // can't beat best known
            if (hF.top() <= hB.top())
                expand(distF, distB, tF, hF, offF, headF, wgtF, mu);
            else
                expand(distB, distF, tB, hB, offB, headB, wgtB, mu);
        }
        return mu == INF ? -1 : (int64_t)mu;
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
    // ---- pass 1: out-degree + in-degree per chunk ----
    std::vector<std::vector<uint32_t>> degO(T, std::vector<uint32_t>(n, 0));
    std::vector<std::vector<uint32_t>> degI(T, std::vector<uint32_t>(n, 0));
    std::vector<const char*> lo(T), hi(T);
    size_t chunk = (gc.p - gb);
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
                    if (*c.p < '0' || *c.p > '9') { ++c.p; continue; }  // trailing LF
                    uint32_t u=(uint32_t)num(c), v=(uint32_t)num(c); num(c);
                    if (u<n) degO[t][u]++;
                    if (v<n) degI[t][v]++;
                }});
        for (auto& x : th) x.join();
    }
    // ---- offsets both directions ----
    std::vector<uint32_t> offF(n + 1), offB(n + 1);
    offF[0] = offB[0] = 0;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t a = 0, b = 0;
        for (int t = 0; t < T; ++t) { a += degO[t][i]; b += degI[t][i]; }
        offF[i+1] = offF[i] + a;
        offB[i+1] = offB[i] + b;
    }
    degO.clear(); degO.shrink_to_fit();
    degI.clear(); degI.shrink_to_fit();
    // ---- pass 2: fill both CSRs via shared atomic cursors ----
    std::vector<uint32_t> headF(m), wgtF(m), headB(m), wgtB(m);
    {
        std::vector<std::atomic<uint32_t>> cu(n), cv(n);
        for (uint32_t i = 0; i < n; ++i) {
            cu[i].store(offF[i], std::memory_order_relaxed);
            cv[i].store(offB[i], std::memory_order_relaxed);
        }
        std::vector<std::thread> th;
        for (int t = 0; t < T; ++t)
            th.emplace_back([&,t]{
                Cur c{lo[t], hi[t]};
                while (c.p < c.end) {
                    if (*c.p < '0' || *c.p > '9') { ++c.p; continue; }  // trailing LF
                    uint32_t u=(uint32_t)num(c), v=(uint32_t)num(c), w=(uint32_t)num(c);
                    uint32_t e = cu[u].fetch_add(1, std::memory_order_relaxed);
                    headF[e] = v; wgtF[e] = w;
                    uint32_t e2 = cv[v].fetch_add(1, std::memory_order_relaxed);
                    headB[e2] = u; wgtB[e2] = w;
                }
            });
        for (auto& x : th) x.join();
    }
    munmap((void*)gb, gsz);

    std::vector<int64_t> ans(queries.size());
    {
        std::vector<Worker> W(T);
        std::vector<std::thread> th;
        for (int t = 0; t < T; ++t)
            th.emplace_back([&,t]{
                W[t].init(n, offF.data(), headF.data(), wgtF.data(),
                          offB.data(), headB.data(), wgtB.data());
                for (size_t i = t; i < queries.size(); i += T)
                    ans[i] = W[t].query(queries[i].first, queries[i].second);
            });
        for (auto& x : th) x.join();
    }

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
