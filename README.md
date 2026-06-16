# sys-monitor
A minimal system resources monitor.

Reads contents of `/proc` (UNIX) or Mach API (macOS) to calculate resource usage with `curses` output. Snapshots to a `JSON` which can be read by other tools.

```
sys-monitor : uptime  6s: 6m:19h:0d
press 'q' to quit. output: ./monitor.json
     cpu =  17.65% [ ||||||||                                           ]
  memory =  42.72% [ |||||||||||||||||||||                              ]
 load_1m =   1.96% [                                                    ]
 load_5m =   1.78% [                                                    ]
load_15m =   2.01% [ |                                                  ]
```

Output file:
```
{
  "cpu_pct": 17.65,
  "mem_used_b": 3434201088,
  "mem_total_b": 8001028096,
  "mem_used_pct": 42.72,
  "load_1m": 1.96,
  "load_5m": 1.78,
  "load_15m": 2.01,
  "timestamp": 1781637051,
  "uptime": " 6s: 6m:19h:0d"
}
```
