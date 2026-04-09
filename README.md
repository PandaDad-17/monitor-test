# Monitor Test
A C++23 standard, high-performance, cross-platform systems monitoring agent designed to track active window telemetry with low resource usage. It provides real-time window focus events via a decoupled IPC (Inter-Process Communication) architecture.

This project is more proof of concept, as there is no consumer of logic sent over the IPC transport layer. It's just that when that data is sent, it is also printed to the terminal, but it does also accurately print failures to connect. It is also assuming local-only logic, and memory accessed only from its own running process. However, it is still built atomically safe. 

---

## Overview

The project consists of a core library and a demo agent that observes the host operating system's window manager. When the user switches focus, the agent captures the window title and transmits it to a hypothetical collector service.

* **Windows Implementation:** Leverages `SetWinEventHook` and a non-polling message pump to receive OS signals with 0% idle CPU usage.
* **Linux Implementation:** Utilizes X11 `XGetInputFocus` with extended window manager hints support for UTF-8 titles.
* **IPC Transport:** Uses unix domain sockets on Linux and named pipes on Windows for low-latency and local-only communication.

---

## Technical Goals

This codebase is a showcase of modern C++ systems programming, adhering to strict performance and safety constraints.

### 1. Modern C++23 Standards
The project is built against the **C++23** standard:
* **`std::expected`**: Used for error handling in the transport and monitor layers to avoid exceptions but still providing detailed error handling.
* **`std::jthread` & `std::stop_token`**: Ensures clean thread lifecycle management and proper handling of interuption.
* **`std::print` / `std::println`**: Leveraging the new C++23 formatted output library for efficient, type-safe logging.

### 2. RAII & The Rule of Five
Resource management is strictly handled through RAII to ensure zero memory leaks:
* **Custom Deleters**: C pointers are wrapped in `std::unique_ptr` with custom deleters to ensure cleanup.
* **Rule of Five**: Classes are designed to be either non-copyable or follow the Rule of Five and manage move semantics, preventing double-closing of sockets or handles.

### 3. Zero-Copy & Memory Efficiency
Designed for the "Silent Agent" philosophy, the project avoids the heap wherever possible:
* **Stack-Based Conversion:** UTF-16 to UTF-8 conversions on Windows occur in fixed-size stack buffers to avoid high-frequency heap allocations during focus changes.
* **`std::string_view`**: Used throughout the pipeline to pass string data without duplicating memory.
* **Vectored I/O**: On Linux, we use `iovec` to send message payloads and delimiters in a single kernel call, avoiding temporary string concatenations.

**Note:** Unlike the Linux variant, Windows does have to copy. This is to keep the kernel calls atomically safe. It is technically possible to pass `std::string_view` data and then a new line with two API calls, but if anything else is writing to the pipe at the same time, it could write first before the new line.

### 4. Architectural Patterns
* **Pointer to Implementation:** Used in the `Monitor` class to decouple the public API from platform-specific headers. This reduces compile times and header pollution.
* **Self-Healing IPC:** The transport layer is designed to be resilient, automatically attempting to reconnect to the collector service if the pipe or socket is temporarily unavailable.

---

## Building the Project

### Prerequisites
* **Windows:** Visual Studio 2022+, Latest Visual Studio 2017-2026 Redistributiable, CMake 3.20+.
* **Linux:** GCC 14+ or Clang 18+, X11 development headers (`libx11-dev`).

### Commands
```bash
# Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the targets
cmake --build build --config Release
```

#### **Linux**
```bash
# Start the server
nc -lkU /tmp/monitor.sock

# In a separate terminal
./build/demo_agent
```

#### **Windows**
```bash
# Start the server (accept paste multiple lines warning)
while($true) {
    $p = New-Object System.IO.Pipes.NamedPipeServerStream('monitor')
    $p.WaitForConnection()
    $r = New-Object System.IO.StreamReader($p)
    while($null -ne ($line = $r.ReadLine())) { 
        Write-Host $line 
    }
    $p.Dispose()
}

# In a separate terminal
./build/Release/demo_agent

```

**Note:** It is easiest to run the launch configuration `Full Monitor Stack` in Visual Studio Code, as the commands listed above are automatically run by the process.
