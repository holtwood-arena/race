// Track A v4: v3 + MADV_DONTNEED evict-behind-cursor (bounds file-page RSS)
// + buffered output with manual u32 formatting (kills 127k fprintf calls).
// Build: g++ -O3 -march=native -pthread -o v4_evict v4_evict.cpp
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

struct Group {
    uint32_t count = 0, errors = 0;
    std::vector<uint32_t> lat;
};

struct Local {
    std::unordered_map<std::string_view, uint32_t> epmap;
    std::vector<std::string_view> eps;
    std::unordered_map<uint32_t, Group> groups;
};

static void parse(const char* p, const char* end, Local& L,
                  const char* region_lo) {
    L.epmap.reserve(2048);
    L.eps.reserve(2048);
    L.groups.reserve(1 << 16);
    const char* evict = region_lo;
    const size_t STEP = 4 << 20;
    while (p < end) {
        uint32_t day = (p[8] - '0') * 10 + p[9] - '0';
        uint32_t hh = (p[11] - '0') * 10 + p[12] - '0';
        uint32_t hidx = (day >= 30 ? day - 30 : day + 1) * 24 + hh;
        const char* s1 = (const char*)memchr(p + 28, ' ', end - p - 28);
        const char* s2 = (const char*)memchr(s1 + 1, ' ', end - s1 - 1);
        const char* j = (const char*)memchr(s2 + 5, '\n', end - s2 - 5);
        std::string_view ep(s1 + 1, s2 - s1 - 1);
        auto [it, ins] = L.epmap.try_emplace(ep, (uint32_t)L.eps.size());
        if (ins) L.eps.push_back(ep);
        uint32_t lat = 0;
        for (const char* q = s2 + 5; q < j; ++q) lat = lat * 10 + (*q - '0');
        Group& g = L.groups[hidx * 2048 + it->second];
        g.count++;
        g.errors += (s2[1] == '5');
        g.lat.push_back(lat);
        p = j + 1;
        if (p - evict > (ptrdiff_t)STEP) {
            uintptr_t a = ((uintptr_t)evict + 4095) & ~(uintptr_t)4095;
            uintptr_t b = (uintptr_t)p & ~(uintptr_t)4095;
            if (b > a) madvise((void*)a, b - a, MADV_DONTNEED);
            evict = p;
        }
    }
}

static inline char* u32str(uint32_t v, char* o) {
    char tmp[10]; int n = 0;
    do { tmp[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) *o++ = tmp[--n];
    return o;
}

int main(int argc, char** argv) {
    std::string data_dir = "/data", out_dir = "/out";
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--data")) data_dir = argv[i + 1];
        else if (!strcmp(argv[i], "--out")) out_dir = argv[i + 1];
    }
    int fd = open((data_dir + "/requests.log").c_str(), O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st; fstat(fd, &st);
    const char* base = (const char*)mmap(nullptr, st.st_size,
        PROT_READ, MAP_PRIVATE, fd, 0);
    madvise((void*)base, st.st_size, MADV_SEQUENTIAL);
    const char* end = base + st.st_size;

    const int T = 12;
    std::vector<Local> locals(T);
    std::vector<std::thread> th;
    size_t chunk = st.st_size / T;
    for (int t = 0; t < T; ++t) {
        const char* lo = base + t * chunk;
        const char* hi = (t == T - 1) ? end : base + (t + 1) * chunk;
        if (t > 0) {
            lo = (const char*)memchr(lo, '\n', end - lo);
            lo = lo ? lo + 1 : end;
        }
        if (t < T - 1) {
            const char* b = (const char*)memchr(hi, '\n', end - hi);
            hi = b ? b + 1 : end;
        }
        th.emplace_back(parse, lo, hi, std::ref(locals[t]), lo);
    }
    for (auto& x : th) x.join();
    // mapping stays until exit: endpoint string_views point into it

    std::unordered_map<std::string_view, uint32_t> gep;
    gep.reserve(2048);
    std::vector<std::string_view> eps;
    eps.reserve(2048);
    std::unordered_map<uint32_t, Group> groups;
    groups.reserve(1 << 17);
    for (auto& L : locals) {
        std::vector<uint32_t> remap(L.eps.size());
        for (uint32_t i = 0; i < L.eps.size(); ++i) {
            auto [it, ins] = gep.try_emplace(L.eps[i], (uint32_t)eps.size());
            if (ins) eps.push_back(L.eps[i]);
            remap[i] = it->second;
        }
        for (auto& [k, g] : L.groups) {
            uint32_t gk = (k / 2048) * 2048 + remap[k % 2048];
            Group& G = groups[gk];
            G.count += g.count;
            G.errors += g.errors;
            G.lat.insert(G.lat.end(), g.lat.begin(), g.lat.end());
        }
    }

    std::vector<uint32_t> by_hour[96];
    for (auto& [k, g] : groups) by_hour[k / 2048].push_back(k % 2048);

    std::string out;
    out.reserve(12 << 20);
    for (uint32_t h = 0; h < 96; ++h) {
        auto& v = by_hour[h];
        std::sort(v.begin(), v.end(),
            [&](uint32_t a, uint32_t b) { return eps[a] < eps[b]; });
        uint32_t day = 30 + h / 24, mon = day <= 31 ? 10 : 11;
        char hour[24];
        int hl = snprintf(hour, sizeof hour, "2026-%02u-%02uT%02u:00:00Z",
                          mon, day <= 31 ? day : day - 31, h % 24);
        for (uint32_t epid : v) {
            Group& g = groups[h * 2048 + epid];
            std::sort(g.lat.begin(), g.lat.end());
            uint32_t n = g.count;
            out.append(hour, hl);
            out.push_back(' ');
            out.append(eps[epid]);
            out.push_back(' ');
            char buf[64]; char* o = buf;
            o = u32str(n, o); *o++ = ' ';
            o = u32str(g.lat[(50 * n + 99) / 100 - 1], o); *o++ = ' ';
            o = u32str(g.lat[(95 * n + 99) / 100 - 1], o); *o++ = ' ';
            o = u32str(g.lat[(99 * n + 99) / 100 - 1], o); *o++ = ' ';
            o = u32str(g.errors, o); *o++ = ' ';
            o = u32str((uint32_t)((uint64_t)g.errors * 1000000 / n), o);
            *o++ = '\n';
            out.append(buf, o - buf);
        }
    }
    std::string outpath = out_dir + "/summary.txt";
    FILE* f = fopen(outpath.c_str(), "w");
    if (!f) { perror("fopen"); return 1; }
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return 0;
}
