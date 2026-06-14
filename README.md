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