#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__APPLE__) || defined(__MACH__)
    #define PLATFORM_MACOS
    #include <mach/mach.h>
    #include <mach/mach_host.h>
#elif defined(unix) || defined(__unix__) || defined(__unix)
    #define PLATFORM_UNIX
#elif defined(_WIN32) || defined(_WIN64) || defined(CYGWIN)
    #error "windows build not supported"
#endif

typedef struct {
    double cpu_pct;
    unsigned long mem_used_mb;
    unsigned long mem_total_mb;
    double load_1m;
} snapshot;

typedef struct {
    uint64_t user, system, idle, nice;  // per logical CPU, or aggregate
} cpu_sample_t;

void platform_sample_cpu(cpu_sample_t *out, int ncpu);   // impl differs per OS
int  platform_cpu_count(void);
void write_status(const char* out_path, snapshot* s);

int main(int argc, char* argv[]) {
    if (argc != 1) return -1;

    printf("test %s\n", argv[0]);
    return 0;
}

void write_status(const char* out_path, snapshot* s) {
    // Write to a temp file on the same filesystem, then rename()
    // rename() is atomic on POSIX — the swap is instantaneous
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", out_path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) return;

    fprintf(f,
        "{\n"
        "  \"cpu_pct\": %.1f,\n"
        "  \"mem_used_mb\": %lu,\n"
        "  \"mem_total_mb\": %lu,\n"
        "  \"load_1m\": %.2f,\n"
        "  \"timestamp\": %ld\n"
        "}\n",
        s->cpu_pct, s->mem_used_mb, s->mem_total_mb,
        s->load_1m, (long)time(NULL)
    );

    fflush(f);
    #ifdef PLATFORM_UNIX
    fsync(fileno(f)); // ensure it's on disk before the rename
    #elif defined(PLATFORM_MACOS)
    fcntl(fileno(f), F_FULLFSYNC);
    #endif
    fclose(f);

    rename(tmp_path, out_path); // atomic swap
}
