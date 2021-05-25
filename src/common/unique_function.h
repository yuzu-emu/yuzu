// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <utility>

namespace Common {

template <typename ResultType, typename... Args>
class UniqueFunction {
    class CallableBase {
    public:
        virtual ~CallableBase() = default;
        virtual ResultType operator()(Args...) = 0;
    };

    template <typename Functor>
    class Callable final : public CallableBase {
    public:
        Callable(Functor&& functor_) : functor{std::move(functor_)} {}
        ~Callable() override = default;

        ResultType operator()(Args... args) override {
            return functor(std::forward<Args>(args)...);
        }

    private:
        Functor functor;
    };

public:
    UniqueFunction() = default;

    template <typename Functor>
    UniqueFunction(Functor&& functor)
        : callable{std::make_unique<Callable<Functor>>(std::move(functor))} {}

    UniqueFunction& operator=(UniqueFunction<ResultType, Args...>&& rhs) noexcept {
        callable = std::move(rhs.callable);
        return *this;
    }

    UniqueFunction(UniqueFunction<ResultType, Args...>&& rhs) noexcept
        : callable{std::move(rhs.callable)} {}

    ResultType operator()(Args... args) const {
        return (*callable)(std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept {
        return callable != nullptr;
    }

    UniqueFunction& operator=(const UniqueFunction<ResultType, Args...>&) = delete;
    UniqueFunction(const UniqueFunction<ResultType, Args...>&) = delete;

private:
    std::unique_ptr<CallableBase> callable;
};

} // namespace Common
