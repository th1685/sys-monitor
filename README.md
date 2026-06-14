# sys-monitor
A minimal system resources monitor.

Reads contents of `/proc` (UNIX) or Mach API (macOS) to calculate resource usage with `curses` output. Snapshots to a `JSON` which can be read by other tools.

```
sys-monitor : press 'q' to quit
     cpu =  19.40% [ |||||||||                                          ]
  memory =  31.82% [ |||||||||||||||                                    ]
 load_1m =   4.33% [ ||                                                 ]
 load_5m =   2.89% [ |                                                  ]
load_15m =   2.25% [ |                                                  ]
```

Output file:
```
{
  "cpu_pct": 19.4,
  "mem_used_kb": 2547350395,
  "mem_total_kb": 8005500928,
  "mem_used_pct": 31.82,
  "load_1m": 4.33,
  "load_5m": 2.89,
  "load_15m": 2.25,
  "timestamp": 1781444201
}
```