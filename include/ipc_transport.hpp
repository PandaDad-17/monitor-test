#ifndef __IPC_TRANSPORT_HPP__
#define __IPC_TRANSPORT_HPP__

#include <expected>
#include <memory>
#include <string_view>

#include "monitor.hpp"

namespace core
{

class IPCTransport 
{
public:
  virtual ~IPCTransport() noexcept = default;

  // Rule of five and interfaces should not be copyable or movable.
  IPCTransport(const IPCTransport&) = delete;
  IPCTransport& operator=(const IPCTransport&) = delete;
  IPCTransport(IPCTransport&&) = delete;
  IPCTransport& operator=(IPCTransport&&) = delete;

  [[nodiscard]] virtual std::expected<void, Error> send(std::string_view message) noexcept = 0;

  // Factory method to create the correct version based on the OS.
  [[nodiscard]] static std::unique_ptr<IPCTransport> create(std::string_view address);

protected:
  // Only derived implementations can instantiate this.
  IPCTransport() noexcept = default;
};

} // namespace core

#endif // __IPC_TRANSPORT_HPP__
