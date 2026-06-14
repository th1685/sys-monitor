#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <ncurses.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

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
    unsigned long mem_used_b;
    unsigned long mem_total_b;
    double loads[3];
} snapshot;

typedef struct {
    uint64_t user, nice, system, idle;  // per logical CPU, or aggregate
} cpu_sample_t;

typedef struct {
    uint64_t user_d, nice_d, system_d, idle_d, total_d, active_d;
} cpu_delta_t;

typedef struct {
    uint64_t total, free, active, inactive, wired, compressed;
} mem_sample_t;


int sample_loop(snapshot* s, cpu_delta_t* cpu0, mem_sample_t* mem0, long interval);
int sample_cpu(cpu_sample_t *out);   // impl differs per OS
int sample_memory(mem_sample_t *out);
void ts_add_ms(struct timespec* out, long ms);
int sampler_run(cpu_delta_t* d, long interval_ms);
uint64_t safe_substitution(uint64_t new, uint64_t old);
cpu_delta_t cpu_delta(const cpu_sample_t* new, const cpu_sample_t* old);
double cpu_usage(const cpu_delta_t* d);
const char* progress_bar(double pct, int width);
void printw_status_line(char* name, double stat, const char* bar);
void write_status(const char* out_path, snapshot* s);
void init_curses(void);


int main(int argc, char* argv[]) { //sysmon -f "/file/path/to/log" -v for ncurses output
    if (argc > 3) { return -1; }

    const char* default_filepath = "./monitor.json";
    const char* filepath = default_filepath;

    int ncurses_output = 0;
    int opt;

    while ((opt = getopt(argc, argv, "hvf:")) != -1) {
        switch (opt) {
            case 'h':
                printf("-v: ncurses output\n"
                       "-f: specify filepath\n");
                return 0; 
            case 'v':
                ncurses_output = 1;
                break;
            case 'f':
                filepath = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-h] [-v] [-f file]\n", argv[0]);
                return -1;
        }
    }

    cpu_delta_t cpu0 = {0};
    mem_sample_t mem0 = {0};
    snapshot s = {0};
    long interval = 1000;
    int progress_bar_width = 50;
    int ch;

    if (ncurses_output) { 
        init_curses();
        printw("sys-monitor : press 'q' to quit\noutput: %s\n", filepath);

        while ((ch = getch()) != 'q') {
            move(2, 0);

            if (sample_loop(&s, &cpu0, &mem0, interval) != 0) return -1;

            double mem_used_pct = 100.0 * (double)s.mem_used_b / (double)s.mem_total_b;

            printw_status_line("cpu", s.cpu_pct, progress_bar(s.cpu_pct, progress_bar_width));
            printw_status_line("memory", mem_used_pct, progress_bar(mem_used_pct, progress_bar_width));
            printw_status_line("load_1m", s.loads[0], progress_bar(s.loads[0], progress_bar_width));
            printw_status_line("load_5m", s.loads[1], progress_bar(s.loads[1], progress_bar_width));
            printw_status_line("load_15m", s.loads[2], progress_bar(s.loads[2], progress_bar_width));
            refresh();
            write_status(filepath, &s);
        }

        endwin();			/* End curses mode		  */

    } else {
        while(1) {
            if (sample_loop(&s, &cpu0, &mem0, interval) != 0) return -1;
            write_status(filepath, &s);
        }
    }

    return 0;
}


int sample_loop(snapshot* s, cpu_delta_t* c, mem_sample_t* m, long interval) {
    if (sampler_run(c, interval) != 0) {printf("could not sample cpu\n"); return -1;}
    if (sample_memory(m) != 0) {printf("could not sample memory\n"); return -1;}

    s->cpu_pct = cpu_usage(c);
    s->mem_used_b = m->active + m->wired;

    #if defined(PLATFORM_MACOS)
    s->mem_total_b = m->active + m->inactive + m->free + m->wired + m->compressed;
    #elif defined(PLATFORM_UNIX)
    s->mem_total_b = mem0->total;
    #endif
    
    getloadavg(s->loads, 3);

    return 0;
}


#if defined(PLATFORM_UNIX)
int sample_cpu(cpu_sample_t* out) {
    /*user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice*/
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    const int MAX_LINE_LENGTH = 128;
    char line[MAX_LINE_LENGTH], core_name[32];

    fgets(line, MAX_LINE_LENGTH, f);

    if (sscanf(line, "%15s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, 
                core_name, &out->user, &out->nice, &out->system, &out->idle) != 5) {
        printf("could not parse /proc/stat\n");
        return -1;
    }

    fclose(f);
    return 0;
}


int sample_memory(mem_sample_t* out) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    const int MAX_LINE_LENGTH = 128;
    char line[MAX_LINE_LENGTH];
    int i = 0;

    out->compressed = 0;

    while (fgets(line, MAX_LINE_LENGTH, f)) {
        if      (sscanf(line, "MemTotal: %"  PRIu64 " kB", &out->total)    == 1) {}
        else if (sscanf(line, "MemFree: %"   PRIu64 " kB", &out->free)     == 1) {}
        else if (sscanf(line, "Active: %"    PRIu64 " kB", &out->active)   == 1) {}
        else if (sscanf(line, "Inactive: %"  PRIu64 " kB", &out->inactive) == 1) {}
    }

    fclose(f);
    return 0;
}


#elif defined(PLATFORM_MACOS)
int sample_cpu(cpu_sample_t *out) {
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t cpu_count = HOST_CPU_LOAD_INFO_COUNT;

    kern_return_t cpu_kr = host_statistics64(mach_host_self(),
                                        HOST_CPU_LOAD_INFO,
                                        (host_info64_t)&info,
                                        &cpu_count);

    if (cpu_kr != KERN_SUCCESS) {
        fprintf(stderr, "host_statistics64(HOST_CPU_LOAD_INFO) failed: %s\n",
                mach_error_string(cpu_kr));
        return -1;
    }
    
    out->user = (uint64_t)info.cpu_ticks[CPU_STATE_USER];
    out->nice = (uint64_t)info.cpu_ticks[CPU_STATE_NICE];
    out->system = (uint64_t)info.cpu_ticks[CPU_STATE_SYSTEM];
    out->idle = (uint64_t)info.cpu_ticks[CPU_STATE_IDLE];

    return 0;
}


int sample_memory(mem_sample_t* out) {
    vm_statistics64_data_t vm;
    mach_msg_type_number_t vm_count = HOST_VM_INFO64_COUNT;

    kern_return_t vm_kr = host_statistics64(mach_host_self(),
                                        HOST_VM_INFO64,
                                        (host_info64_t)&vm,
                                        &vm_count);

    if (vm_kr != KERN_SUCCESS) {
        fprintf(stderr, "host_statistics64(HOST_VM_INFO64) failed: %s\n",
                mach_error_string(vm_kr));
        return -1;
    }

    mach_port_t host = mach_host_self();
    vm_size_t page_size = 0;
    vm_kr = host_page_size(host, &page_size);
    if (vm_kr != KERN_SUCCESS) {
        fprintf(stderr, "host_page_size failed: %s\n", mach_error_string(vm_kr));
        return -1;
    }

    out->total = 0;
    out->free = (uint64_t)vm.free_count * page_size;
    out->active = (uint64_t)vm.active_count * page_size;
    out->inactive = (uint64_t)vm.inactive_count * page_size;
    out->wired = (uint64_t)vm.wire_count * page_size;
    out->compressed = (uint64_t)vm.compressor_page_count * page_size;

    return 0;
}
#endif


void ts_add_ms(struct timespec* ts, long ms) {
    ts->tv_nsec += ms * 1000000L;
    ts->tv_sec  += ts->tv_nsec / 1000000000L;
    ts->tv_nsec %= 1000000000L;
}


int sampler_run(cpu_delta_t* d, long interval_ms) {
    cpu_sample_t prev, now;
    struct timespec ts = {
        .tv_sec  = interval_ms / 1000,
        .tv_nsec = (interval_ms % 1000) * 1000000L,
    };

    if (sample_cpu(&prev) != 0) return -1;
    nanosleep(&ts, NULL);
    if (sample_cpu(&now) != 0) return -1;

    *d = cpu_delta(&now, &prev);
    return 0;
}


uint64_t safe_substitution(uint64_t new, uint64_t old) {
    return (new >= old) ? new - old : 0;
}


cpu_delta_t cpu_delta(const cpu_sample_t* new, const cpu_sample_t* old) {
    cpu_delta_t d = {
        .user_d   = safe_substitution(new->user, old->user),
        .nice_d   = safe_substitution(new->nice, old->nice),
        .system_d = safe_substitution(new->system, old->system),
        .idle_d   = safe_substitution(new->idle, old->idle),
    };

    d.total_d = d.user_d + d.system_d + d.idle_d + d.nice_d;
    d.active_d = d.user_d + d.system_d + d.nice_d;

    return d;
}


double cpu_usage(const cpu_delta_t* d) {
    return d->total_d ? 100.00 * (double)d->active_d / (double)d->total_d : 0.0;
}


const char* progress_bar(double pct, int width) {
    static char bar[1024];

    int progress = (int)(pct * width / 100.0);

    if (width < 0) width = 0;
    if (progress < 0) progress = 0;
    if (progress > width) progress = width;

    int i = 0;

    //bar[i++] = '[';

    for (int j = 0; j < width; j++) {
        bar[i++] = (j < progress) ? '|' : ' ';
    }

    //bar[i++] = ']';
    bar[i] = '\0';

    return bar;
}


void printw_status_line(char* name, double stat, const char* bar) {
    attron(COLOR_PAIR(1));
    printw("%8s = %6.2f%% [ ", name, stat);
    if (stat < 100.00 && stat >= 80.0) {
        attron(COLOR_PAIR(2));
    } else if (stat < 80.0 && stat >= 50.0) {
        attron(COLOR_PAIR(3));
    } else if (stat < 50.0 && stat >= 0.0) {
        attron(COLOR_PAIR(4));
    } else {
        attron(COLOR_PAIR(5));
    }
    printw("%s", bar);
    attron(COLOR_PAIR(1));
    printw(" ]\n");
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
        "  \"cpu_pct\": %.2f,\n"
        "  \"mem_used_b\": %lu,\n"
        "  \"mem_total_b\": %lu,\n"
        "  \"mem_used_pct\": %.2f,\n"
        "  \"load_1m\": %.2f,\n"
        "  \"load_5m\": %.2f,\n"
        "  \"load_15m\": %.2f,\n"
        "  \"timestamp\": %ld\n"
        "}\n",
        s->cpu_pct, s->mem_used_b, s->mem_total_b,
        100.0 * (double)s->mem_used_b / (double)s->mem_total_b,
        s->loads[0], s->loads[1], s->loads[2], (long)time(NULL)
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

void init_curses(void) {
    initscr();			/* Start curses mode 		  */
    curs_set(0); /*hide cursor*/
    noecho();
    nodelay(stdscr, TRUE);
    timeout(250);

    if (has_colors()) {
        start_color();
        use_default_colors();

        init_color(COLOR_YELLOW, 1000, 750, 0);

        init_pair(1, -1, -1);
        init_pair(2, COLOR_RED, -1);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_GREEN, -1);
        init_pair(5, COLOR_BLUE, -1);
    }
}