#include "ipc_transport.hpp"
#include "monitor.hpp"

#include <atomic>
#include <format>
#include <iostream>
#include <print>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// The pipe address on Windows uses the UNC naming convention.
static constexpr std::string_view DefaultIPCPath = R"(\\.\pipe\monitor)";

// Global boolean to handle Ctrl+C.
std::atomic<bool> quit_demo{ false };

// This handles Ctrl+C, Ctrl+Break, and closing the terminal window.
BOOL WINAPI console_handler(DWORD signal)
{
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_LOGOFF_EVENT)
  {
    quit_demo.store(true);
    quit_demo.notify_one();
    return TRUE;
  }
  return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
  // Register the control handler for shut down.
  if (!SetConsoleCtrlHandler(console_handler, TRUE))
  {
    MessageBoxA(nullptr, "Monitor: Could not set control handler.", "Monitor Error", MB_ICONERROR | MB_OK);
    return ERROR_CONTROL_C_EXIT;
  }

  // Launch console.
  AllocConsole();
  FILE* fDummy;
  freopen_s(&fDummy, "CONOUT$", "w", stdout);
  freopen_s(&fDummy, "CONOUT$", "w", stderr);

  // Initialize Windows Named Pipe transport.
  auto transport = core::IPCTransport::create(DefaultIPCPath);

  // Callback for window change events.
  auto on_window_change = [&transport](std::string_view app_name)
  {
    if (auto result = transport->send(app_name); !result) [[unlikely]]
      std::print(std::cerr, "[IPC Error] Failed to send event: {}\n", core::to_string(result.error()));
    else  
      std::println("[SENT] {}", app_name);
  };

  // Initialize and start the monitor.
  core::Monitor monitor;
  if (auto result = monitor.start(on_window_change); !result) [[unlikely]]
  {
    MessageBoxA(nullptr, "Failed to initialize Monitor.", "Monitor Error", MB_ICONERROR | MB_OK);
    return 1;
  }

  quit_demo.wait(false);

  return 0;
}