#include "ipc_transport.hpp"
#include "monitor.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <print>
#include <string_view>

static constexpr std::string_view DefaultIPCPath = "/tmp/monitor.sock";

// Global boolean to handle Ctrl+C.
std::atomic<bool> quit_demo{ false };

void signal_handler(int)
{
  quit_demo.store(true);
  quit_demo.notify_one();
}

int main()
{
  // Setup signal handling to shut down.
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Prevent kernel from killing process with SIGPIPE to stop the agent from crashing if the IPC socket closes
  // unexpectedly. We handle disconnection errors locally within the Transport layer's write calls.
  std::signal(SIGPIPE, SIG_IGN);

  std::println("Monitor agent starting... (press Ctrl+C to stop)");

  // Initialize transport layer.
  auto transport = core::IPCTransport::create(DefaultIPCPath);

  // Callback for window change event.
  auto on_window_change = [&transport](std::string_view app_name)
  {
    if (auto result = transport->send(app_name); !result) [[unlikely]]
      std::print(std::cerr, "[IPC Error] Failed to send event: {}\n", core::to_string(result.error()));
    else  
      std::println("[SENT] {}", app_name);
  };

  // Start and run the agent.
  core::Monitor monitor;
  auto result = monitor.start(on_window_change);
  if (!result)
  {
    std::print(std::cerr, "Initialization failed: {}\n", core::to_string(result.error()));
    return 1;
  }

  // Keep running until ready to quit demo and stop the agent. Block the main thread until then.
  quit_demo.wait(false);

  // The Monitor destructor will handle stop() automatically.
  std::println("Monitor agent shutting down... cleanup complete.");

  return 0;
}
