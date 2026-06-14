cmake_minimum_required(VERSION 3.16)
project(sys-monitor)

add_executable(sysmon main.c)
target_link_libraries(sysmon ncurses)

target_sources(sysmon
  PRIVATE
    main.c
)