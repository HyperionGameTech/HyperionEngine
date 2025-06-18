/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef STRING_UTIL_HPP
#define STRING_UTIL_HPP

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>

namespace hyperion {
namespace utilities {

class StringUtil
{
public:
    static inline bool StartsWith(const std::string& text, const std::string& token)
    {
        if (text.length() < token.length())
        {
            return false;
        }
        return (text.compare(0, token.length(), token) == 0);
    }

    static inline bool EndsWith(const std::string& text, const std::string& token)
    {
        if (text.length() < token.length())
        {
            return false;
        }

        return std::equal(text.begin() + text.size() - token.size(),
            text.end(), token.begin());
    }

    static inline bool Contains(const std::string& text, const std::string& token)
    {
        return text.find(token) != std::string::npos;
    }

    static inline Array<std::string> Split(const std::string& text, char sep)
    {
        Array<std::string> tokens;

        std::string working_string;
        working_string.reserve(text.length());

        for (char ch : text)
        {
            if (ch == sep)
            {
                tokens.PushBack(working_string);
                working_string.clear();
                continue;
            }

            working_string += ch;
        }

        if (!working_string.empty())
        {
            tokens.PushBack(working_string);
        }

        return tokens;
    }

    static inline Array<std::string> RemoveEmpty(const Array<std::string>& strings)
    {
        Array<std::string> res;
        res.Reserve(strings.Size());

        for (const auto& str : strings)
        {
            if (!str.empty())
            {
                res.PushBack(str);
            }
        }

        return res;
    }

    static inline std::string TrimLeft(const std::string& s)
    {
        std::string res(s);
        res.erase(res.begin(), std::find_if(res.begin(), res.end(), [](int c)
                                   {
                                       return !std::isspace(c);
                                   }));
        return res;
    }

    static inline std::string TrimRight(const std::string& s)
    {
        std::string res(s);
        res.erase(std::find_if(res.rbegin(), res.rend(),
                      [](int c)
                      {
                          return !std::isspace(c);
                      })
                      .base(),
            res.end());
        return res;
    }

    static inline std::string Trim(const std::string& s)
    {
        return TrimLeft(TrimRight(s));
    }

    template <size_t Size>
    static inline std::string Join(const std::array<std::string, Size>& args, const std::string& join_by)
    {
        const size_t count = args.size();

        std::stringstream ss;
        size_t i = 0;

        for (auto& str : args)
        {
            ss << str;

            if (i != count - 1 && !EndsWith(str, join_by))
            {
                ss << join_by;
            }

            i++;
        }

        return ss.str();
    }

    static inline std::string ReplaceAll(const std::string& text,
        const std::string& from, const std::string& to)
    {
        std::string result(text);
        if (from.empty())
        {
            return result;
        }
        size_t start_pos = 0;
        while ((start_pos = text.find(from, start_pos)) != std::string::npos)
        {
            result.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return result;
    }

    static inline std::string Basename(const std::string& filepath)
    {
        if (!Contains(filepath, "/") && !Contains(filepath, "\\"))
        {
            return filepath;
        }

        return filepath.substr(filepath.find_last_of("\\/") + 1);
    }

    static inline std::string BasePath(const std::string& filepath)
    {
        if (!Contains(filepath, "/") && !Contains(filepath, "\\"))
        {
            return "";
        }

        return filepath.substr(0, filepath.find_last_of("\\/"));
    }

    static inline Array<String> CanonicalizePath(const Array<String>& original)
    {
        Array<String> res;

        for (const auto& str : original)
        {
            if (str == ".." && !res.Empty())
            {
                res.PopBack();
            }
            else if (str != ".")
            {
                res.PushBack(str);
            }
        }

        return res;
    }

    static inline Array<std::string> CanonicalizePath(const Array<std::string>& original)
    {
        Array<std::string> res;

        for (const auto& str : original)
        {
            if (str == ".." && !res.Empty())
            {
                res.PopBack();
            }
            else if (str != ".")
            {
                res.PushBack(str);
            }
        }

        return res;
    }

    static inline std::string PathToString(const Array<std::string>& path)
    {
        std::string res;

        for (SizeType i = 0; i < path.Size(); i++)
        {
            res += path[i];

            if (i != path.Size() - 1)
            {
                res += "/";
            }
        }

        return res;
    }

    static inline String StripExtension(const String& filename)
    {
        SizeType last_index = filename.FindLastIndex('.');

        if (last_index == String::not_found)
        {
            return filename;
        }

        return filename.Substr(0, last_index);
    }

    static inline String GetExtension(const String& path)
    {
        Array<String> split_path = path.Split('/', '\\');

        if (split_path.Empty())
        {
            return "";
        }

        const String& filename = split_path.Back();

        SizeType last_index = filename.FindLastIndex('.');

        if (last_index == String::not_found)
        {
            return "";
        }

        return filename.Substr(last_index + 1);
    }

    static inline String ToPascalCase(const String& str, bool preserve_case = false)
    {
        Array<String> parts = str.Split('_', ' ', '-');

        for (SizeType i = 0; i < parts.Size(); i++)
        {
            String& part = parts[i];

            if (part.Empty())
            {
                continue;
            }

            if (!preserve_case)
            {
                part = String(part.Substr(0, 1)).ToUpper() + String(part.Substr(1)).ToLower();
            }
            else
            {
                part = String(part.Substr(0, 1)).ToUpper() + String(part.Substr(1));
            }
        }

        return String::Join(parts, "");
    }

    static inline bool Parse(const String& str, int* out_value)
    {
        *out_value = std::strtol(str.Data(), nullptr, 0);

        return true;
    }

    static inline bool Parse(const String& str, long* out_value)
    {
        *out_value = std::strtol(str.Data(), nullptr, 0);

        return true;
    }

    static inline bool Parse(const String& str, long long* out_value)
    {
        *out_value = std::strtoll(str.Data(), nullptr, 0);

        return true;
    }

    static inline bool Parse(const String& str, unsigned int* out_value)
    {
        unsigned int val = 0;
        const char* p = str.Data();

        while (*p)
        {
            val = (val << 1) + (val << 3) + *(p++) - 48;
        }

        *out_value = val;

        return true;
    }

    static inline bool Parse(const String& str, float* out_value)
    {
        *out_value = std::strtof(str.Data(), nullptr);

        return true;
    }

    static inline bool Parse(const String& str, double* out_value)
    {
        *out_value = std::strtod(str.Data(), nullptr);

        return true;
    }

    template <typename T>
    static inline bool Parse(const String& str, T* out_value)
    {
        std::istringstream ss(str.Data());
        T value;

        if (!(ss >> std::boolalpha >> value))
        {
            return false;
        }

        *out_value = value;

        return true;
    }

    template <typename T>
    static inline T Parse(const String& str, T value_on_error = T {})
    {
        T value = value_on_error;

        Parse(str, &value);

        return value;
    }

    template <typename T>
    static inline bool IsNumber(const String& str)
    {
        T value {};

        return Parse<T>(str, &value);
    }
};

} // namespace utilities

using utilities::StringUtil;

} // namespace hyperion

#endif
