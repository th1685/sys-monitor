cmake_minimum_required(VERSION 3.16)
project(sys-monitor)

add_executable(sysmon)

target_sources(sysmon
  PRIVATE
    main.c
)