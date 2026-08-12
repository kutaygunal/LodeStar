// core/adapters/Json.h
// Minimal self-contained JSON value type with parse + serialize (Phase 5).
// Supports object, array, string, number, bool, null. No external deps.

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace lodestar {

// Minimal JSON value. Throws JsonError on type mismatches / parse errors.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() : type_(Type::Null) {}
    static Json null() { return Json(); }
    static Json boolean(bool b);
    static Json number(double n);
    static Json string(const std::string& s);
    static Json array();
    static Json object();

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool() const;
    double asNumber() const;
    const std::string& asString() const;
    std::vector<Json>& asArray();
    const std::vector<Json>& asArray() const;
    std::map<std::string, Json>& asObject();
    const std::map<std::string, Json>& asObject() const;

    // Object accessors.
    bool has(const std::string& key) const;
    Json& operator[](const std::string& key);
    const Json& at(const std::string& key) const;

    // Array accessors.
    Json& operator[](size_t i);
    void push(const Json& v);
    size_t size() const;

    std::string dump() const;

    // Parse a JSON document. Throws JsonError on malformed input.
    static Json parse(const std::string& text);

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::shared_ptr<std::vector<Json>> arr_;
    std::shared_ptr<std::map<std::string, Json>> obj_;
};

class JsonError : public std::runtime_error {
public:
    explicit JsonError(const std::string& m) : std::runtime_error(m) {}
};

}  // namespace lodestar
