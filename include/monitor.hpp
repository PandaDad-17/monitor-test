#ifndef __MONITOR_HPP__
#define __MONITOR_HPP__

#include <expected>
#include <functional>
#include <memory>
#include <string_view>

namespace core
{

static constexpr std::string_view UnknownWindowName = "Unknown";

enum class Error : uint8_t
{
  OSHookFailed,
  IPCConnectionLost,
  AccessDenied
};

[[nodiscard]] inline constexpr std::string_view to_string(Error e) noexcept
{
  switch (e)
  {
    case Error::OSHookFailed:      return "OS Hook Failed";
    case Error::IPCConnectionLost: return "IPC Connection Lost";
    case Error::AccessDenied:      return "Access Denied";
    default:                       [[unlikely]] return "Unknown Error";
  }
}

class Monitor final
{
public:
  using EventCallback = std::function<void(std::string_view)>;

  Monitor();
  ~Monitor() noexcept;

  // Don't allow multiple hooks by disabling copy.
  Monitor(const Monitor&) = delete;
  Monitor& operator=(const Monitor&) = delete;

  // Adhere to rule of five.
  Monitor(Monitor&& o) noexcept = default;
  Monitor& operator=(Monitor&& o) noexcept = default;

  [[nodiscard]] std::expected<void, Error> start(EventCallback callback);

  void stop() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> pimpl;
};

} // namespace core

#endif // __MONITOR_HPP__
