#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

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
    uint64_t free, active, inactive, wired;
} mem_sample_t;


void sample_cpu(cpu_sample_t *out);   // impl differs per OS
void sample_memory(mem_sample_t *out);
void write_status(const char* out_path, snapshot* s);


int main(int argc, char* argv[]) {
    if (argc != 1) return -1;

    cpu_sample_t cpu0 = {0}, cpu1 = {0};
    mem_sample_t mem0 = {0}, mem1 = {0};

    sample_memory(&mem1);

    printf("free: %llu\n", mem1.free);
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
    #if defined(PLATFORM_UNIX)
    fsync(fileno(f)); // ensure it's on disk before the rename
    #elif defined(PLATFORM_MACOS)
    fcntl(fileno(f), F_FULLFSYNC);
    #endif
    fclose(f);

    rename(tmp_path, out_path); // atomic swap
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

void mem_sample(mem_sample_t* out) {
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