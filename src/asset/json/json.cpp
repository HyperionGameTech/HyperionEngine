#include "json.h"

#include <sstream>

namespace hyperion {
namespace json {

std::string JSONArray::ToString(int indent_level) const
{
    std::stringstream ss;
    ss << Indented("[", indent_level);

    for (size_t i = 0; i < m_values.size(); i++) {
        ss << "\n";

        ss << m_values[i].ToString(indent_level + 1);

        if (i != m_values.size() - 1) {
            ss << ",";
        } else {
            ss << "\n";
        }
    }

    ss << Indented("]", indent_level);

    return ss.str();
}

std::string JSONObject::ToString(int indent_level) const
{
    std::stringstream ss;
    ss << Indented("{\n", indent_level);

    size_t counter = 0;

    for (const auto &[k, v] : m_values) {
        ss <<  Indented("\"" + k + "\"", indent_level + 1) << ": ";
        ss << v.ToString(0);

        if (counter != m_values.size() - 1) {
            ss << ",\n";
        }

        counter++;
    }

    ss << "\n";
    ss << Indented("}", indent_level);

    return ss.str();
}

std::string JSONNumber::ToString(int indent_level) const
{
    std::stringstream ss;
    
    switch (num_type) {
    case JSON_NUMBER_INT:
        ss << Indented(std::to_string(int_value), indent_level);

        break;
    case JSON_NUMBER_FLOAT:
        ss << Indented(std::to_string(double_value), indent_level);

        break;
    }

    return ss.str();
}

std::string JSONString::ToString(int indent_level) const
{
    std::stringstream ss;

    ss << Indented(value, indent_level);

    return ss.str();
}

std::string JSONBoolean::ToString(int indent_level) const
{
    std::stringstream ss;

    ss << Indented(value ? "true" : "false", indent_level);

    return ss.str();
}

std::string JSONNull::ToString(int indent_level) const
{
    std::stringstream ss;

    ss << Indented("null", indent_level);

    return ss.str();
}

std::string JSONValue::ToString(int indent_level) const
{
    switch (type) {
    case JSON_NULL:
        return std::get<JSONNull>(value).ToString();
    case JSON_STRING:
        return std::get<JSONString>(value).ToString();
    case JSON_NUMBER:
        return std::get<JSONNumber>(value).ToString();
    case JSON_OBJECT:
        return std::get<JSONObject>(value).ToString();
    case JSON_ARRAY:
        return std::get<JSONArray>(value).ToString();
    case JSON_BOOLEAN:
        return std::get<JSONBoolean>(value).ToString();
    default:
        return "??";
    }
}
} // namespace json
} // namespace hyperion