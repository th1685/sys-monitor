# sys-monitor
A minimal system resources monitor.

Reads contents of `/proc` (UNIX) / Mach API (macOS) to calculate resource usage. Optional terminal `curses` mode. Snapshots to a `JSON` which can be read by other tools.