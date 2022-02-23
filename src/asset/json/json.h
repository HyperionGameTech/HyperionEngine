#ifndef HYPERION_JSON_H
#define HYPERION_JSON_H

#include "../fbom/fbom.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <variant>

namespace hyperion {
namespace json {

struct JSONValue;

struct JSONArray {
    std::vector<JSONValue> m_values;

    std::string ToString(int indent_level = 0) const;
};

struct JSONObject {
    std::unordered_map<std::string, JSONValue> m_values;

    std::string ToString(int indent_level = 0) const;
};

struct JSONNumber {
    enum {
        JSON_NUMBER_INT,
        JSON_NUMBER_FLOAT
    } num_type;

    union {
        int64_t int_value;
        double double_value;
    };

    JSONNumber() : num_type(JSON_NUMBER_INT), int_value(0) {}
    JSONNumber(int64_t value) : num_type(JSON_NUMBER_INT), int_value(value) {}
    JSONNumber(double value) : num_type(JSON_NUMBER_FLOAT), double_value(value) {}
    JSONNumber(const JSONNumber &other) : num_type(other.num_type)
    {
        switch (other.num_type) {
        case JSON_NUMBER_INT:
            int_value = other.int_value;
            break;
        case JSON_NUMBER_FLOAT:
            double_value = other.double_value;
            break;
        }
    }

    std::string ToString(int indent_level = 0) const;
};

struct JSONString {
    std::string value;

    JSONString() {}
    JSONString(const std::string &value) : value(value) {}
    JSONString(const JSONString &other) : value(other.value) {}

    std::string ToString(int indent_level = 0) const;
};

struct JSONBoolean {
    bool value;

    std::string ToString(int indent_level = 0) const;
};

struct JSONNull {
    std::string ToString(int indent_level = 0) const;
};

struct JSONValue : public fbom::FBOMLoadable {
    enum {
        JSON_NULL,
        JSON_STRING,
        JSON_NUMBER,
        JSON_OBJECT,
        JSON_ARRAY,
        JSON_BOOLEAN
    } type;

    std::variant<
        JSONNull,
        JSONString,
        JSONNumber,
        JSONObject,
        JSONArray,
        JSONBoolean
    > value;

    JSONValue() : type(JSON_NULL), value(JSONNull{}) {}
    JSONValue(const JSONValue &other) : type(other.type), value(other.value) {}

    std::string ToString(int indent_level = 0) const;
};

static inline std::string Indented(const std::string &str, int level)
{
    std::string result;
    result.reserve(str.length() + level);

    for (int i = 0; i < level; i++) {
        result += " ";
    }

    result += str;

    return result;
}


} // namespace json
} // namespace hyperion

#endif