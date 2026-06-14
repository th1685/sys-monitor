# sys-monitor
A minimal system resources monitor.

Reads contents of `/proc` (UNIX) or Mach API (macOS) to calculate resource usage with `curses` output. Snapshots to a `JSON` which can be read by other tools.