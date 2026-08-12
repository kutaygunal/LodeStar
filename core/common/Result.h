#pragma once
// core/common/Result.h
// Lightweight Result<T> type: either a value of type T or a typed error.
//
// WP-F (B2): every Result carries a typed ErrorCode in addition to a message.
// The existing err(std::string) factory maps to ErrorCode::Internal so all
// pre-existing call sites keep working unchanged.

#include <string>
#include <utility>
#include <variant>

namespace lodestar::common {

// Typed error codes (WP-F / B2). None is only ever returned for successful
// results; every failure carries one of the concrete codes below.
enum class ErrorCode {
    None = 0,
    InvalidArgument,
    NotFound,
    Duplicate,
    IntegrityViolation,
    IllegalTransition,
    ValidationFailed,
    DatabaseError,
    IoError,
    MigrationError,
    BackupError,
    ConcurrencyError,
    LimitExceeded,
    Internal
};

namespace detail {
struct ErrorInfo {
    ErrorCode code = ErrorCode::Internal;
    std::string message;
};
}  // namespace detail

template <typename T>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(std::string message) {
        return Result(detail::ErrorInfo{ErrorCode::Internal, std::move(message)});
    }
    static Result err(ErrorCode code, std::string message) {
        return Result(detail::ErrorInfo{code, std::move(message)});
    }

    bool isOk() const { return std::holds_alternative<T>(data_); }
    bool failed() const { return !isOk(); }

    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }

    const std::string& error() const { return std::get<detail::ErrorInfo>(data_).message; }
    ErrorCode errorCode() const {
        return isOk() ? ErrorCode::None : std::get<detail::ErrorInfo>(data_).code;
    }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(detail::ErrorInfo info) : data_(std::move(info)) {}

    std::variant<T, detail::ErrorInfo> data_;
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
        return err(ErrorCode::Internal, std::move(message));
    }
    static Result err(ErrorCode code, std::string message) {
        Result r;
        r.ok_ = false;
        r.code_ = code;
        r.error_ = std::move(message);
        return r;
    }

    bool isOk() const { return ok_; }
    bool failed() const { return !ok_; }

    std::string& value() { return value_; }
    const std::string& value() const { return value_; }
    const std::string& error() const { return error_; }
    ErrorCode errorCode() const { return ok_ ? ErrorCode::None : code_; }

private:
    Result() = default;
    bool ok_ = false;
    ErrorCode code_ = ErrorCode::None;
    std::string value_;
    std::string error_;
};

// Specialization for void results.
template <>
class Result<void> {
public:
    static Result ok() { return Result(true); }
    static Result err(std::string message) {
        return err(ErrorCode::Internal, std::move(message));
    }
    static Result err(ErrorCode code, std::string message) {
        Result r;
        r.error_ = std::move(message);
        r.code_ = code;
        return r;
    }

    bool isOk() const { return error_.empty(); }
    bool failed() const { return !isOk(); }
    const std::string& error() const { return error_; }
    ErrorCode errorCode() const { return isOk() ? ErrorCode::None : code_; }

private:
    Result() = default;
    explicit Result(bool) : error_() {}
    std::string error_;
    ErrorCode code_ = ErrorCode::None;
};

}  // namespace lodestar::common
