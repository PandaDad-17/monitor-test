#include "monitor.hpp"

#include <atomic>
#include <iostream>
#include <thread>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace core
{

struct Monitor::Impl
{
  struct XDisplayDeleter
  {
    void operator()(Display *d) const noexcept
    {
      if (d) [[likely]]
        XCloseDisplay(d);
    }
  };

  // We must join before closing the display, otherwise the loop might try to use a null pointer. So this must be
  // declared before the thread to ensure it is destroyed after the thread (which will ensure the thread is stopped).
  std::unique_ptr<Display, XDisplayDeleter> display{nullptr};
  std::jthread observer_thread;
  EventCallback event_cb;

  void process_window_event(Window w)
  {
    if (!event_cb) [[unlikely]]
      return;

    // Attempt to check _NET_WM_NAME first, which allows for utf-8 encoding.
    Atom net_wm_name = XInternAtom(display.get(), "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(display.get(), "UTF8_STRING", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop_name = nullptr;
    int result = XGetWindowProperty(
      display.get(),
      w,
      net_wm_name,
      0,              // offset
      1024,           // length
      False,          // delete
      utf8_string,
      &actual_type,
      &actual_format,
      &nitems,
      &bytes_after,
      &prop_name
    );

    if ((result == Success) && prop_name)
    {
      std::unique_ptr<char, decltype(&XFree)> name_ptr(reinterpret_cast<char*>(prop_name), XFree);
      event_cb(std::string_view(name_ptr.get()));
      return;
    }

    // Fallback to legacy WM_NAME.
    char *name = nullptr;
    if (XFetchName(display.get(), w, &name) && name) [[likely]]
    {
      std::unique_ptr<char, decltype(&XFree)> name_ptr(name, XFree);
      event_cb(std::string_view(name_ptr.get()));
    }
    else [[unlikely]]
      event_cb(UnknownWindowName);
  }

  void execute_observation(std::stop_token token)
  {
    Window current_focus = 0;
    int revert_to;
    while (!token.stop_requested())
    {
      Window new_focus;
      if (XGetInputFocus(display.get(), &new_focus, &revert_to) != 0) [[likely]]
      {
        if ((new_focus != current_focus) && (new_focus != None))
        {
          current_focus = new_focus;
          process_window_event(current_focus);
        }
      }

      // Polling interval: 5Hz for UI monitoring.
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
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

  pimpl->display.reset(XOpenDisplay(nullptr));
  if (!pimpl->display) [[unlikely]]
    return std::unexpected(Error::OSHookFailed);

  pimpl->event_cb = std::move(callback);
  
  pimpl->observer_thread = std::jthread([ptr = pimpl.get()](std::stop_token token) { ptr->execute_observation(token); });

  return {};
}

void Monitor::stop() noexcept
{
  pimpl->observer_thread = {};
  pimpl->display.reset(nullptr);
}

} // namespace core
