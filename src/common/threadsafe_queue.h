// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// a simple lockless thread-safe,
// single reader, single writer queue

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <utility>

namespace Common {
template <typename T, bool with_stop_token = false>
class SPSCQueue {
public:
    SPSCQueue() {
        write_ptr = read_ptr = new ElementPtr();
    }
    ~SPSCQueue() {
        // this will empty out the whole queue
        delete read_ptr;
    }

    [[nodiscard]] std::size_t Size() const {
        return size.load();
    }

    [[nodiscard]] bool Empty() const {
        return Size() == 0;
    }

    [[nodiscard]] T& Front() const {
        return read_ptr->current;
    }

    template <typename Arg>
    void Push(Arg&& t) {
        // create the element, add it to the queue
        write_ptr->current = std::forward<Arg>(t);
        // set the next pointer to a new element ptr
        // then advance the write pointer
        ElementPtr* new_ptr = new ElementPtr();
        write_ptr->next.store(new_ptr, std::memory_order_release);
        write_ptr = new_ptr;

        const size_t previous_size{size++};

        // Acquire the mutex and then immediately release it as a fence.
        // TODO(bunnei): This can be replaced with C++20 waitable atomics when properly supported.
        // See discussion on https://github.com/yuzu-emu/yuzu/pull/3173 for details.
        if (previous_size == 0) {
            std::lock_guard lock{cv_mutex};
        }
        cv.notify_one();
    }

    void Pop() {
        --size;

        ElementPtr* tmpptr = read_ptr;
        // advance the read pointer
        read_ptr = tmpptr->next.load();
        // set the next element to nullptr to stop the recursive deletion
        tmpptr->next.store(nullptr);
        delete tmpptr; // this also deletes the element
    }

    bool Pop(T& t) {
        if (Empty()) {
            return false;
        }
        --size;

        ElementPtr* tmpptr = read_ptr;
        read_ptr = tmpptr->next.load(std::memory_order_acquire);
        t = std::move(tmpptr->current);
        tmpptr->next.store(nullptr);
        delete tmpptr;
        return true;
    }

    T PopWait(std::stop_token stop_token) {
        if (Empty()) {
            std::unique_lock lock{cv_mutex};
            cv.wait(lock, stop_token, [this] { return !Empty(); });
            if (stop_token.stop_requested()) {
                return T{};
            }
        }
        T t;
        Pop(t);
        return t;
    }

    T PopWait() {
        if (Empty()) {
            std::unique_lock lock{cv_mutex};
            cv.wait(lock, [this] { return !Empty(); });
        }
        T t;
        Pop(t);
        return t;
    }

    // not thread-safe
    void Clear() {
        size.store(0);
        delete read_ptr;
        write_ptr = read_ptr = new ElementPtr();
    }

private:
    // stores a pointer to element
    // and a pointer to the next ElementPtr
    class ElementPtr {
    public:
        ElementPtr() {}
        ~ElementPtr() {
            ElementPtr* next_ptr = next.load();

            if (next_ptr)
                delete next_ptr;
        }

        T current;
        std::atomic<ElementPtr*> next{nullptr};
    };

    ElementPtr* write_ptr;
    ElementPtr* read_ptr;
    std::atomic_size_t size{0};
    std::mutex cv_mutex;
    std::conditional_t<with_stop_token, std::condition_variable_any, std::condition_variable> cv;
};

// a simple thread-safe,
// single reader, multiple writer queue

template <typename T, bool with_stop_token = false>
class MPSCQueue {
public:
    [[nodiscard]] std::size_t Size() const {
        return spsc_queue.Size();
    }

    [[nodiscard]] bool Empty() const {
        return spsc_queue.Empty();
    }

    [[nodiscard]] T& Front() const {
        return spsc_queue.Front();
    }

    template <typename Arg>
    void Push(Arg&& t) {
        std::lock_guard lock{write_lock};
        spsc_queue.Push(t);
    }

    void Pop() {
        return spsc_queue.Pop();
    }

    bool Pop(T& t) {
        return spsc_queue.Pop(t);
    }

    T PopWait() {
        return spsc_queue.PopWait();
    }

    T PopWait(std::stop_token stop_token) {
        return spsc_queue.PopWait(stop_token);
    }

    // not thread-safe
    void Clear() {
        spsc_queue.Clear();
    }

private:
    SPSCQueue<T, with_stop_token> spsc_queue;
    std::mutex write_lock;
};
} // namespace Common
