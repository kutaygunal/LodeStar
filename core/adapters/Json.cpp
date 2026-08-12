// core/adapters/Json.cpp
// Minimal JSON parse + serialize implementation (Phase 5).

#include "core/adapters/Json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace lodestar {

namespace {
void skipWs(const char*& p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
}

std::string parseString(const char*& p, const char* end) {
    if (p >= end || *p != '"') throw JsonError("expected string");
    ++p;
    std::string out;
    while (p < end && *p != '"') {
        char c = *p++;
        if (c == '\\') {
            if (p >= end) throw JsonError("bad escape");
            char e = *p++;
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (p + 4 > end) throw JsonError("bad \\u");
                    unsigned int code = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = *p++;
                        code = code * 16 + (h <= '9' ? (h - '0') : (h <= 'F' ? h - 'A' + 10 : h - 'a' + 10));
                    }
                    out += static_cast<char>(code & 0xFF);  // BMP only, UTF-8 not fully handled
                    break;
                }
                default: throw JsonError("bad escape");
            }
        } else {
            out += c;
        }
    }
    if (p >= end) throw JsonError("unterminated string");
    ++p;  // closing quote
    return out;
}

Json parseValue(const char*& p, const char* end);

Json parseNumber(const char*& p, const char* end) {
    const char* start = p;
    if (p < end && (*p == '-' || *p == '+')) ++p;
    while (p < end && (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.' ||
                       *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) ++p;
    std::string s(start, p);
    return Json::number(std::atof(s.c_str()));
}

Json parseValue(const char*& p, const char* end) {
    skipWs(p, end);
    if (p >= end) throw JsonError("unexpected end");
    char c = *p;
    if (c == '{') {
        ++p;
        Json obj = Json::object();
        skipWs(p, end);
        if (p < end && *p == '}') { ++p; return obj; }
        while (true) {
            skipWs(p, end);
            std::string key = parseString(p, end);
            skipWs(p, end);
            if (p >= end || *p != ':') throw JsonError("expected ':'");
            ++p;
            obj[key] = parseValue(p, end);
            skipWs(p, end);
            if (p >= end) throw JsonError("unexpected end in object");
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; break; }
            throw JsonError("expected ',' or '}'");
        }
        return obj;
    }
    if (c == '[') {
        ++p;
        Json arr = Json::array();
        skipWs(p, end);
        if (p < end && *p == ']') { ++p; return arr; }
        while (true) {
            arr.push(parseValue(p, end));
            skipWs(p, end);
            if (p >= end) throw JsonError("unexpected end in array");
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; break; }
            throw JsonError("expected ',' or ']'");
        }
        return arr;
    }
    if (c == '"') {
        return Json::string(parseString(p, end));
    }
    if (c == 't') { if (end - p >= 4 && std::string(p, p + 4) == "true") { p += 4; return Json::boolean(true); } throw JsonError("bad true"); }
    if (c == 'f') { if (end - p >= 5 && std::string(p, p + 5) == "false") { p += 5; return Json::boolean(false); } throw JsonError("bad false"); }
    if (c == 'n') { if (end - p >= 4 && std::string(p, p + 4) == "null") { p += 4; return Json::null(); } throw JsonError("bad null"); }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(p, end);
    throw JsonError(std::string("unexpected char '") + c + "'");
}
}  // namespace

Json Json::boolean(bool b) { Json j; j.type_ = Type::Bool; j.bool_ = b; return j; }
Json Json::number(double n) { Json j; j.type_ = Type::Number; j.num_ = n; return j; }
Json Json::string(const std::string& s) { Json j; j.type_ = Type::String; j.str_ = s; return j; }
Json Json::array() { Json j; j.type_ = Type::Array; j.arr_ = std::make_shared<std::vector<Json>>(); return j; }
Json Json::object() { Json j; j.type_ = Type::Object; j.obj_ = std::make_shared<std::map<std::string, Json>>(); return j; }

bool Json::asBool() const { if (type_ != Type::Bool) throw JsonError("not a bool"); return bool_; }
double Json::asNumber() const { if (type_ != Type::Number) throw JsonError("not a number"); return num_; }
const std::string& Json::asString() const { if (type_ != Type::String) throw JsonError("not a string"); return str_; }
std::vector<Json>& Json::asArray() { if (type_ != Type::Array || !arr_) throw JsonError("not an array"); return *arr_; }
const std::vector<Json>& Json::asArray() const { if (type_ != Type::Array || !arr_) throw JsonError("not an array"); return *arr_; }
std::map<std::string, Json>& Json::asObject() { if (type_ != Type::Object || !obj_) throw JsonError("not an object"); return *obj_; }
const std::map<std::string, Json>& Json::asObject() const { if (type_ != Type::Object || !obj_) throw JsonError("not an object"); return *obj_; }

bool Json::has(const std::string& key) const {
    if (type_ != Type::Object || !obj_) return false;
    return obj_->find(key) != obj_->end();
}
Json& Json::operator[](const std::string& key) {
    if (type_ != Type::Object || !obj_) throw JsonError("not an object");
    return (*obj_)[key];
}
const Json& Json::at(const std::string& key) const {
    if (type_ != Type::Object || !obj_) throw JsonError("not an object");
    auto it = obj_->find(key);
    if (it == obj_->end()) throw JsonError("key not found: " + key);
    return it->second;
}
Json& Json::operator[](size_t i) {
    if (type_ != Type::Array || !arr_) throw JsonError("not an array");
    return (*arr_)[i];
}
void Json::push(const Json& v) {
    if (type_ != Type::Array || !arr_) throw JsonError("not an array");
    arr_->push_back(v);
}
size_t Json::size() const {
    if (type_ == Type::Array) return arr_ ? arr_->size() : 0;
    if (type_ == Type::Object) return obj_ ? obj_->size() : 0;
    return 0;
}

std::string Json::dump() const {
    switch (type_) {
        case Type::Null: return "null";
        case Type::Bool: return bool_ ? "true" : "false";
        case Type::Number: {
            if (std::fabs(num_ - std::floor(num_)) < 1e-9 && std::fabs(num_) < 1e15)
                return std::to_string(static_cast<long long>(num_));
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.10g", num_);
            return std::string(buf);
        }
        case Type::String: {
            std::string out = "\"";
            for (char c : str_) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out += c;
                }
            }
            return out + "\"";
        }
        case Type::Array: {
            std::string out = "[";
            if (arr_) {
                for (size_t i = 0; i < arr_->size(); ++i) {
                    if (i) out += ",";
                    out += (*arr_)[i].dump();
                }
            }
            return out + "]";
        }
        case Type::Object: {
            std::string out = "{";
            if (obj_) {
                bool first = true;
                for (const auto& kv : *obj_) {
                    if (!first) out += ",";
                    first = false;
                    out += Json::string(kv.first).dump() + ":" + kv.second.dump();
                }
            }
            return out + "}";
        }
    }
    return "null";
}

Json Json::parse(const std::string& text) {
    const char* p = text.c_str();
    const char* end = p + text.size();
    Json v = parseValue(p, end);
    skipWs(p, end);
    if (p != end) throw JsonError("trailing data after JSON");
    return v;
}

}  // namespace lodestar
