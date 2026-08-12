#pragma once
// core/common/Result.h
// Lightweight Result<T> type: either a value of type T or an error string.

#include <string>
#include <utility>
#include <variant>

namespace lodestar::common {

template <typename T>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(std::string message) { return Result(std::move(message)); }

    bool isOk() const { return std::holds_alternative<T>(data_); }
    bool failed() const { return !isOk(); }

    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }

    const std::string& error() const { return std::get<std::string>(data_); }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(std::string message) : data_(std::move(message)) {}

    std::variant<T, std::string> data_;
};

// Specialization for std::string results. The primary template stores both the
// value and the error as std::string, which would be ambiguous (and an
// ill-formed duplicate-variant) when T == std::string, so it gets its own layout.
template <>
class Result<std::string> {
public:
    static Result ok(std::string value) {
        Result r;
        r.ok_ = true;
        r.value_ = std::move(value);
        return r;
    }
    static Result err(std::string message) {
        Result r;
        r.ok_ = false;
        r.error_ = std::move(message);
        return r;
    }

    bool isOk() const { return ok_; }
    bool failed() const { return !ok_; }

    std::string& value() { return value_; }
    const std::string& value() const { return value_; }
    const std::string& error() const { return error_; }

private:
    Result() = default;
    bool ok_ = false;
    std::string value_;
    std::string error_;
};

// Specialization for void results.
template <>
class Result<void> {
public:
    static Result ok() { return Result(true); }
    static Result err(std::string message) { return Result(std::move(message)); }

    bool isOk() const { return error_.empty(); }
    bool failed() const { return !isOk(); }
    const std::string& error() const { return error_; }

private:
    explicit Result(bool) : error_() {}
    explicit Result(std::string message) : error_(std::move(message)) {}
    std::string error_;
};

}  // namespace lodestar::common
