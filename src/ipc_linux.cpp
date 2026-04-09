#include "ipc_transport.hpp"

#include <cstring>
#include <string>
#include <utility>

#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

namespace core 
{

// RAII wrapper for linux socket file descriptor. 
class UniqueSocket final
{
  int socket_fd{ -1 };
public:
  explicit UniqueSocket(int fd = -1) noexcept : socket_fd(fd) 
  {
  }
  ~UniqueSocket() noexcept
  {
    reset();
  }
  
  // Mimic unique_ptr behavior.
  UniqueSocket(const UniqueSocket&) = delete;
  UniqueSocket& operator=(const UniqueSocket&) = delete;
  UniqueSocket(UniqueSocket&& other) noexcept : socket_fd(std::exchange(other.socket_fd, -1)) 
  {
  }
  UniqueSocket& operator=(UniqueSocket&& other) noexcept
  {
    if (this != &other)
      reset(std::exchange(other.socket_fd, -1));
    return *this;
  }

  void reset(int new_fd = -1) noexcept
  {
    if (socket_fd != -1)
      ::close(socket_fd);
    socket_fd = new_fd;
  }

  [[nodiscard]] int get() const noexcept
  {
    return socket_fd;
  }
  
  [[nodiscard]] bool is_valid() const noexcept
  {
    return socket_fd != -1;
  }
};

// Linux implementation using Unix Domain Sockets.
class LinuxTransport final : public IPCTransport
{
  UniqueSocket socket_ptr;
  const std::string path;

public:
    LinuxTransport(std::string_view socket_path) : path(socket_path)
    {
    }

    std::expected<void, Error> connect() noexcept
    {
      // Unix domain, stream-oriented socket that guarantees the bytes arrive in the order they were sent.
      socket_ptr.reset(socket(AF_UNIX, SOCK_STREAM, 0));
      if (!socket_ptr.is_valid()) [[unlikely]]
        return std::unexpected(Error::IPCConnectionLost);

      sockaddr_un addr{};
      addr.sun_family = AF_UNIX;
      if (path.size() >= sizeof(addr.sun_path)) [[unlikely]]
      {
        socket_ptr.reset();
        return std::unexpected(Error::AccessDenied);
      }
      std::memcpy(addr.sun_path, path.c_str(), path.size());

      if (::connect(socket_ptr.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
      {
        socket_ptr.reset();
        return std::unexpected(Error::IPCConnectionLost);
      }

      return {};
    }

    [[nodiscard]] std::expected<void, Error> send(std::string_view message) noexcept override
    {
      // Try a recconnect if connection lost.
      if (!socket_ptr.is_valid()) [[unlikely]]
      {
        if (auto result = this->connect(); !result)
          return result;
      }

      struct iovec data[2];

      // Avoid copy by writing the data stream directly. As ugly as this is, we know the vectors are only going to be
      // used in ::writev and not modified.
      data[0].iov_base = const_cast<void*>(static_cast<const void*>(message.data()));
      data[0].iov_len  = message.size();

      // We send the string plus a newline as a delimiter. Same const casting is necessary for a const char* 
      // string literal.
      data[1].iov_base = const_cast<void*>(static_cast<const void*>("\n"));
      data[1].iov_len  = 1;
      
      // 'writev' returns bytes written.
      const ssize_t total_bytes = static_cast<ssize_t>(message.size() + 1);
      if (::writev(socket_ptr.get(), data, 2) < total_bytes)
      {
        // Failure to write is likely a broken pipe. Self-heal by reseting on next attempt.
        socket_ptr.reset();
        return std::unexpected(Error::IPCConnectionLost);
      }
        
      return {};
    }
  };

  std::unique_ptr<IPCTransport> IPCTransport::create(std::string_view address)
  {
    auto transport = std::make_unique<LinuxTransport>(address);
    
    // Don't handle errors here as there is self-healing (reconnecting) and error handling in send.
    (void)transport->connect();
    
    return transport;
  }

} // namespace core
