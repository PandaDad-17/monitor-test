#include "monitor.hpp"

#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace core
{

struct Monitor::Impl
{
  struct HookDeleter
  {
    void operator()(HWINEVENTHOOK h) const noexcept
    {
      if (h)
        ::UnhookWinEvent(h);
    }
  };

  // Singleton reference to implmentation for the required global Windows API event callback.
  static inline Impl* instance = nullptr;

  // Static callback required by Windows API
  static void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD dwEventThread,
    DWORD dwmsEventTime
  )
  {
    if (event == EVENT_SYSTEM_FOREGROUND)
    {
      if (instance)
        instance->process_window_event(hwnd);
    }
  }

  std::unique_ptr<std::remove_pointer_t<HWINEVENTHOOK>, HookDeleter> hook{ nullptr };
  std::jthread observer_thread;
  EventCallback event_cb;
  std::promise<std::expected<void, Error>> startup_promise;

  void process_window_event(HWND hwnd)
  {
    if (!hwnd || !event_cb) [[unlikely]]
      return;

    wchar_t utf16_buf[256];
    int length = GetWindowTextW(hwnd, utf16_buf, static_cast<int>(std::size(utf16_buf)));
    if (length > 0)
    {
      // One utf-8 can take up to 4 bytes to represent a utf-16 character. 
      char utf8_buf[1024];

      int bytes_written = WideCharToMultiByte(
        CP_UTF8,                                // CodePage
        0,                                      // dwFlags
        utf16_buf,                              // lpWideCharStr
        length,                                 // cchWideChar
        utf8_buf,                               // lpMultiByteStr
        static_cast<int>(std::size(utf16_buf)), // cbMultiByte  
        nullptr,                                // lpDefaultChar
        nullptr                                 // lpUsedDefaultChar
      );

      if (bytes_written > 0) [[likely]]
      {
        event_cb(std::string_view(utf8_buf, bytes_written));
        return;
      }
    }

    event_cb(UnknownWindowName);
  }

  void execute_observation(std::stop_token token)
  {
    instance = this;

    // Install the hook specifically for foreground changes
    hook.reset(SetWinEventHook(
      EVENT_SYSTEM_FOREGROUND,  // eventMin - only wake thread for forground process
      EVENT_SYSTEM_FOREGROUND,  // eventMax
      nullptr,                  // hmodWinEventProc
      WinEventProc,             // pfnWinEventProc
      0,                        // idProcess
      0,                        // idThread
      WINEVENT_OUTOFCONTEXT     // dwFlags - run hook in our process memory
    ));

    if (!hook)
    {
      instance = nullptr;
      startup_promise.set_value(std::unexpected(Error::OSHookFailed));
      return;
    }
    startup_promise.set_value({});

    // A stop_callback will wake up the message loop immediately when the thread is stopped by using the stop token.
    std::stop_callback cb(token, [] { PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0); });

    // Get message blocks the thread and sleeps until a message arrives.
    // It also only returns zero on WM_QUIT.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    
    instance = nullptr;
  }
};

Monitor::Monitor() : pimpl(std::make_unique<Impl>())
{
}

Monitor::~Monitor() noexcept
{
  stop();
}

std::expected<void, Error> Monitor::start(EventCallback callback)
{
  // If already running don't start again.
  if (pimpl->observer_thread.joinable())
    return {};

  pimpl->event_cb = std::move(callback);
  auto future = pimpl->startup_promise.get_future();
  
  // We use an atomic or a sync primitive here to ensure hook success
  pimpl->observer_thread = std::jthread(
    [ptr = pimpl.get()](std::stop_token token) { ptr->execute_observation(token); }
  );

  // Wait for thread to successfully get hook.
  return future.get();
}

void Monitor::stop() noexcept
{
  pimpl->observer_thread = {}; 
}

} // namespace core
