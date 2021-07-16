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

#include "common/idle_inhibitor.h"

#include <cstdlib>

#include "common/logging/log.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__)
#include <chrono>
#include <dlfcn.h>
#include <unistd.h>
struct DBusConnection;
struct DBusError;
struct DBusMessage;
using dbus_bool_t = uint32_t;
using dbus_uint32_t = uint32_t;
#endif

namespace {
#if defined(__unix__)
constexpr char g_app[] = "yuzu";
constexpr char g_reason[] = "Emulation is running";
#endif

#if defined(__unix__)
constexpr int DBUS_TYPE_INVALID = '\0';
constexpr int DBUS_TYPE_STRING = 's';
constexpr int DBUS_TYPE_UINT32 = 'u';
constexpr int DBUS_TIMEOUT_USE_DEFAULT = -1;
enum DBusBusType {
    DBUS_BUS_SESSION,
    DBUS_BUS_SYSTEM,
    DBUS_BUS_STARTER,
};

struct DBus {
    DBusConnection* (*bus_get)(DBusBusType type, DBusError* error);
    void (*connection_unref)(DBusConnection* connection);
    DBusMessage* (*message_new_method_call)(const char* destination, const char* path,
                                            const char* iface, const char* method);
    dbus_bool_t (*message_append_args)(DBusMessage* message, int first_arg_type, ...);
    dbus_bool_t (*connection_send)(DBusConnection* connection, DBusMessage* message,
                                   dbus_uint32_t* serial);
    DBusMessage* (*connection_send_with_reply_and_block)(DBusConnection* connection,
                                                         DBusMessage* message,
                                                         int timeout_milliseconds,
                                                         DBusError* error);
    dbus_bool_t (*message_get_args)(DBusMessage* message, DBusError* error, int first_arg_type,
                                    ...);
    void (*message_unref)(DBusMessage* message);

    bool initialized = false;
    void* handle = nullptr;

    DBus() {
        handle = dlopen("libdbus-1.so.3", RTLD_NOW);
        if (handle) {
#define dbus_symbol(sym)                                                                           \
    ((sym = reinterpret_cast<decltype(sym)>(dlsym(handle, "dbus_" #sym))), sym != nullptr)
            initialized = dbus_symbol(bus_get);
            initialized |= dbus_symbol(connection_unref);
            initialized |= dbus_symbol(message_new_method_call);
            initialized |= dbus_symbol(message_append_args);
            initialized |= dbus_symbol(connection_send);
            initialized |= dbus_symbol(connection_send_with_reply_and_block);
            initialized |= dbus_symbol(message_get_args);
            initialized |= dbus_symbol(message_unref);
#undef dbus_symbol
            if (!initialized) {
                LOG_ERROR(Common, "Failed to load libdbus functions");
            }
        } else {
            LOG_WARNING(Common, "Failed to dlopen() libdbus-1.so.3");
        }
    }

    ~DBus() {
        if (handle) {
            dlclose(handle);
        }
    }
};

DBus g_dbus;
#endif
} // namespace

namespace Common {
IdleInhibitor::IdleInhibitor() {
#if defined(__unix__)
    ConnectDBus();
#endif
}

IdleInhibitor::~IdleInhibitor() {
    if (inhibiting_idle) {
        AllowIdle();
    }
#ifdef __unix__
    if (dbus_connection) {
        g_dbus.connection_unref(dbus_connection);
    }
#endif
}

#if defined(_WIN32)
void IdleInhibitor::InhibitIdle() {
    inhibiting_idle = true;
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
}

void IdleInhibitor::AllowIdle() {
    inhibiting_idle = false;
    SetThreadExecutionState(ES_CONTINUOUS);
}

#elif defined(__unix__)
void IdleInhibitor::ResetScreensaver() {
    auto poll_time = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (true) {
        std::unique_lock<std::mutex> lk(cond_mutex);
        poll_cond.wait_until(lk, poll_time, [this]() { return !inhibiting_idle; });
        if (!inhibiting_idle) {
            return;
        }

        if (fork() == 0) {
            execlp("xdg-screensaver", "xdg-screensaver", "reset", nullptr);
        }

        poll_time += std::chrono::seconds(10);
    }
}

void IdleInhibitor::ConnectDBus() {
    if (!g_dbus.initialized) {
        return;
    }

    dbus_connection = g_dbus.bus_get(DBUS_BUS_SESSION, nullptr);

    if (!dbus_connection) {
        LOG_WARNING(Common, "Failed to connect to session bus");
    }
}

void IdleInhibitor::InhibitIdle() {
    if (inhibiting_idle) {
        return;
    }

    inhibiting_idle = true;

    if (dbus_connection) {
        DBusMessage* inhibit_call = g_dbus.message_new_method_call(
            "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
            "org.freedesktop.ScreenSaver", "Inhibit");
        if (!inhibit_call) {
            LOG_ERROR(Common, "Failed to create Inhibit call");
            return;
        }

        // dbus_message_append_args() takes char** for string arguments
        const char* app_ptr = &g_app[0];
        const char* reason_ptr = &g_reason[0];
        if (!g_dbus.message_append_args(inhibit_call, DBUS_TYPE_STRING, &app_ptr, DBUS_TYPE_STRING,
                                        &reason_ptr, DBUS_TYPE_INVALID)) {
            g_dbus.message_unref(inhibit_call);
            LOG_ERROR(Common, "Failed to append args to Inhibit call");
            return;
        }

        DBusMessage* reply = g_dbus.connection_send_with_reply_and_block(
            dbus_connection, inhibit_call, DBUS_TIMEOUT_USE_DEFAULT, nullptr);
        g_dbus.message_unref(inhibit_call);

        if (reply) {
            if (!g_dbus.message_get_args(reply, nullptr, DBUS_TYPE_UINT32, &inhibit_cookie)) {
                LOG_ERROR(Common, "Failed to get cookie from Inhibit reply");
                inhibit_cookie = 0;
            }
        } else {
            LOG_WARNING(Common, "Inhibit call failed");
        }
    }

    if (inhibit_cookie == 0) {
        LOG_WARNING(Common, "Falling back to xdg-screensaver polling thread");
        poll_thread = std::make_unique<std::thread>(&Common::IdleInhibitor::ResetScreensaver, this);
    }
}

void IdleInhibitor::AllowIdle() {
    if (!inhibiting_idle) {
        return;
    }

    {
        std::lock_guard lock{cond_mutex};
        inhibiting_idle = false;
    }

    if (poll_thread && poll_thread->joinable()) {
        poll_cond.notify_all();
        poll_thread->join();
        poll_thread.reset();
    }

    if (dbus_connection && inhibit_cookie != 0) {
        DBusMessage* uninhibit_call = g_dbus.message_new_method_call(
            "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
            "org.freedesktop.ScreenSaver", "UnInhibit");
        if (!uninhibit_call) {
            LOG_ERROR(Common, "Failed to create UnInhibit call");
            return;
        }

        if (!g_dbus.message_append_args(uninhibit_call, DBUS_TYPE_UINT32, &inhibit_cookie,
                                        DBUS_TYPE_INVALID)) {
            LOG_ERROR(Common, "Failed to append args to UnInhibit call");
            g_dbus.message_unref(uninhibit_call);
            return;
        }

        if (!g_dbus.connection_send(dbus_connection, uninhibit_call, nullptr)) {
            LOG_WARNING(Common, "UnInhibit call failed");
        }
        g_dbus.message_unref(uninhibit_call);
        inhibit_cookie = 0;
    }
}
#endif
} // namespace Common
