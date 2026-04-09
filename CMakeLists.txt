cmake_minimum_required(VERSION 3.20)
project(monitor-test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-stdlib=libc++ -fexperimental-library)
  add_link_options(-stdlib=libc++)
endif()

add_library(monitor_core)
target_include_directories(monitor_core PUBLIC include)

if(WIN32)
  target_sources(monitor_core PRIVATE
    src/monitor_win.cpp
    src//ipc_win.cpp
  )
  target_link_libraries(monitor_core PRIVATE User32.lib)
  add_executable(demo_agent WIN32 examples/demo_agent_win.cpp)
elseif(UNIX AND NOT APPLE)
  target_sources(monitor_core PRIVATE
    src/monitor_linux.cpp
    src//ipc_linux.cpp
  )
  find_package(X11 REQUIRED)
  target_link_libraries(monitor_core PRIVATE X11::X11)
  add_executable(demo_agent examples/demo_agent_linux.cpp)
endif()

target_link_libraries(demo_agent PRIVATE monitor_core)
