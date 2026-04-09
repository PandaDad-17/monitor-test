#include "ipc_transport.hpp"

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace core
{

class UniqueHandle 
{
  HANDLE h = INVALID_HANDLE_VALUE;
public:
  explicit UniqueHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : h(handle)
  {
  }
  ~UniqueHandle() noexcept
  {
    reset();
  }

  // Mimic unique_ptr behavior.
  UniqueHandle(const UniqueHandle&) = delete;
  UniqueHandle& operator=(const UniqueHandle&) = delete;
  UniqueHandle(UniqueHandle&& o) noexcept : h(std::exchange(o.h, INVALID_HANDLE_VALUE)) 
  {
  }
  UniqueHandle& operator=(UniqueHandle&& o) noexcept
  {
    if (this != &o)
      reset(std::exchange(o.h, INVALID_HANDLE_VALUE));
    return *this;
  }

  void reset(HANDLE new_h = INVALID_HANDLE_VALUE) noexcept 
  {
    if (h != INVALID_HANDLE_VALUE)
      ::CloseHandle(h);
    h = new_h;
  }

  [[nodiscard]] HANDLE get() const noexcept
  { 
    return h;
  }

  [[nodiscard]] bool is_valid() const noexcept
  { 
    return h != INVALID_HANDLE_VALUE;
  }
};

class WinNamedPipeTransport final : public IPCTransport
{
  UniqueHandle pipe_handle;
  const std::string pipe_name;

public:
  explicit WinNamedPipeTransport(std::string_view address) : pipe_name(address)
  {
  }

  std::expected<void, Error> connect() noexcept
  {
    // Close existing handle if we are reconnecting.
    pipe_handle.reset();

    HANDLE h = ::CreateFileA(
      pipe_name.c_str(),  // lpFileName
      GENERIC_WRITE,      // dwDesiredAccess
      0,                  // dwShareMode - No sharing
      nullptr,            // lpSecurityAttributes - Default security
      OPEN_EXISTING,      // dwCreationDisposition - don't create new pipe, only hook into sever already running
      0,                  // dwFlagsAndAttributes
      nullptr             // hTemplateFile
    );

    if (h == INVALID_HANDLE_VALUE) [[unlikely]]
      return std::unexpected(Error::IPCConnectionLost);

    pipe_handle.reset(h);
    return {};
  }

  [[nodiscard]] std::expected<void, Error> send(std::string_view message) noexcept override
  {
    if (!pipe_handle.is_valid()) [[unlikely]]
    {
      if (auto result = connect(); !result) 
        return result;
    }

    // We send the message + newline as the delimiter.
    // Maximum length of Monitor::Impl::process_window_event is 1024, so that plus room for a new line. 
    char framed_message[1025];
    std::memcpy(framed_message, message.data(), message.size());
    std::memcpy(framed_message + message.size(), "\n", 1);
    
    DWORD message_length = static_cast<DWORD>(message.size() + 1);
    DWORD bytes_written = 0;
    BOOL success = ::WriteFile(
      pipe_handle.get(),
      framed_message,
      message_length,
      &bytes_written,
      nullptr
    );

    if (!success || (bytes_written != message_length)) [[unlikely]]
    {
      pipe_handle.reset();
      return std::unexpected(Error::IPCConnectionLost);
    }

    return {};
  }
};

std::unique_ptr<IPCTransport> IPCTransport::create(std::string_view address)
{
  auto transport = std::make_unique<WinNamedPipeTransport>(address);
  // Don't handle errors here as there is self-healing (reconnecting) and error handling in send.
  (void)transport->connect();
  
  return transport;
}

} // namespace core