#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#if defined(__APPLE__) || defined(__MACH__)
    #define PLATFORM_MACOS
    #include <mach/mach.h>
    #include <mach/mach_host.h>
    #include <inttypes.h>
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

typedef struct {
    uint64_t user_d, system_d, idle_d, nice_d, total_d, active_d;
} cpu_delta_t;

typedef struct {
    uint64_t free, active, inactive, wired;
} mem_sample_t;


void sample_cpu(cpu_sample_t *out);   // impl differs per OS
void sample_memory(mem_sample_t *out);
static void ts_add_ms(struct timespec* out, long ms);
static int sleep_until(const struct timespec *deadline);
void sampler_run(long interval_ms);
static uint64_t safe_substitution(uint64_t new, uint64_t old);
cpu_delta_t cpu_delta(const cpu_sample_t* new, const cpu_sample_t* old);
double cpu_usage(const cpu_delta_t* d);
void write_status(const char* out_path, snapshot* s);


int main(int argc, char* argv[]) {
    if (argc != 1) return -1;

    cpu_sample_t cpu0 = {0}, cpu1 = {0};
    mem_sample_t mem0 = {0}, mem1 = {0};

    sample_memory(&mem1);

    printf("free: %llu\n", mem1.free);
    return 0;
}


#if defined(PLATFORM_UNIX)
#elif defined(PLATFORM_MACOS)
void sample_cpu(cpu_sample_t *out) {
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t cpu_count = HOST_CPU_LOAD_INFO_COUNT;

    kern_return_t cpu_kr = host_statistics64(mach_host_self(),
                                        HOST_CPU_LOAD_INFO,
                                        (host_info64_t)&info,
                                        &cpu_count);

    if (cpu_kr != KERN_SUCCESS) {
        fprintf(stderr, "host_statistics64(HOST_CPU_LOAD_INFO) failed: %s\n",
                mach_error_string(cpu_kr));
        return;
    }
    
    out->user = (uint64_t)info.cpu_ticks[CPU_STATE_USER];
    out->system = (uint64_t)info.cpu_ticks[CPU_STATE_IDLE];
    out->idle = (uint64_t)info.cpu_ticks[CPU_STATE_IDLE];
    out->nice = (uint64_t)info.cpu_ticks[CPU_STATE_NICE];
}


void sample_memory(mem_sample_t* out) {
    vm_statistics64_data_t vm;
    mach_msg_type_number_t vm_count = HOST_VM_INFO64_COUNT;

    kern_return_t vm_kr = host_statistics64(mach_host_self(),
                                        HOST_VM_INFO64,
                                        (host_info64_t)&vm,
                                        &vm_count);

    if (vm_kr != KERN_SUCCESS) {
        fprintf(stderr, "host_statistics64(HOST_VM_INFO64) failed: %s\n",
                mach_error_string(vm_kr));
        return;
    }

    mach_port_t host = mach_host_self();
    vm_size_t page_size = 0;
    vm_kr = host_page_size(host, &page_size);
    if (vm_kr != KERN_SUCCESS) {
        fprintf(stderr, "host_page_size failed: %s\n", mach_error_string(vm_kr));
        return;
    }

    out->free = (uint64_t)vm.free_count * page_size;
    out->active = (uint64_t)vm.active_count * page_size;
    out->inactive = (uint64_t)vm.inactive_count * page_size;
    out->wired = (uint64_t)vm.wire_count * page_size;
}
#endif


static void ts_add_ms(struct timespec* ts, long ms) {
    ts->tv_nsec += ms * 1000000L;
    ts->tv_sec  += ts->tv_nsec / 1000000000L;
    ts->tv_nsec %= 1000000000L;
}


static int sleep_until(const struct timespec *deadline) {
    for (;;) {
        struct timespec now, remaining;
        clock_gettime(CLOCK_MONOTONIC, &now);

        remaining.tv_sec  = deadline->tv_sec  - now.tv_sec;
        remaining.tv_nsec = deadline->tv_nsec - now.tv_nsec;
        if (remaining.tv_nsec < 0) {
            remaining.tv_sec--;
            remaining.tv_nsec += 1000000000L;
        }

        if (remaining.tv_sec < 0)
            return 0;  /* deadline already passed, continue immediately */

        if (nanosleep(&remaining, NULL) == 0)
            return 0;

        if (errno != EINTR)
            return -1; /* real error */

        /* EINTR: loop and recompute from the absolute deadline,
           rather than trusting the `rem` output of nanosleep. */
    }
}


void sampler_run(long interval_ms) {
    cpu_sample_t prev, now;
    struct timespec deadline;
    
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    sample_cpu(&prev);

    for (;;) {
        ts_add_ms(&deadline, interval_ms);
        if (sleep_until(&deadline) != 0)
            break;
        sample_cpu(&now);

        cpu_delta_t d = cpu_delta(&now, &prev);

        prev = now;
    }
}


static uint64_t safe_substitution(uint64_t new, uint64_t old) {
    return (new >= old) ? new - old : 0.0;
}


cpu_delta_t cpu_delta(const cpu_sample_t* new, const cpu_sample_t* old) {
    cpu_delta_t d = {
        .user_d   = safe_substitution(new->user, old->user),
        .system_d = safe_substitution(new->system, old->system),
        .idle_d   = safe_substitution(new->idle, old->idle),
        .nice_d   = safe_substitution(new->nice, old->nice),
    };

    d.total_d = d.user_d + d.system_d + d.idle_d + d.nice_d;
    d.active_d = d.user_d + d.system_d + d.nice_d;

    return d;
}


double cpu_usage(const cpu_delta_t* d) {
    return d->total_d ? 100.00 * (double)d->active_d / (double)d->total_d : 0.0;
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
    #if defined(PLATFORM_UNIX)
    fsync(fileno(f)); // ensure it's on disk before the rename
    #elif defined(PLATFORM_MACOS)
    fcntl(fileno(f), F_FULLFSYNC);
    #endif
    fclose(f);

    rename(tmp_path, out_path); // atomic swap
}