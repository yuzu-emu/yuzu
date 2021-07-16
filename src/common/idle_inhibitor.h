// Copyright © 2021 Steven A. Wilson

// Permission to use, copy, modify, and/or distribute this software
// for any purpose with or without fee is hereby granted.

// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
// WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
// AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
// CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
// NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#pragma once

#include <atomic>

#if defined(__unix__)
#include <condition_variable>
#include <mutex>
#include <thread>
#endif

struct DBus;
struct DBusConnection;

namespace Common {
class IdleInhibitor {
public:
    explicit IdleInhibitor();
    ~IdleInhibitor();
    void InhibitIdle();
    void AllowIdle();

private:
    std::atomic_bool inhibiting_idle{false};

#if defined(__unix__)
    void ConnectDBus();
    void ResetScreensaver();

    uint32_t inhibit_cookie = 0;
    DBusConnection* dbus_connection = NULL;
    std::unique_ptr<std::thread> poll_thread;
    std::condition_variable poll_cond;
    std::mutex cond_mutex;
#endif
};
} // namespace Common
