/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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

        std::string workingString;
        workingString.reserve(text.length());

        for (char ch : text)
        {
            if (ch == sep)
            {
                tokens.PushBack(workingString);
                workingString.clear();
                continue;
            }

            workingString += ch;
        }

        if (!workingString.empty())
        {
            tokens.PushBack(workingString);
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
    static inline std::string Join(const std::array<std::string, Size>& args, const std::string& joinBy)
    {
        const size_t count = args.size();

        std::stringstream ss;
        size_t i = 0;

        for (auto& str : args)
        {
            ss << str;

            if (i != count - 1 && !EndsWith(str, joinBy))
            {
                ss << joinBy;
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
        size_t startPos = 0;
        while ((startPos = text.find(from, startPos)) != std::string::npos)
        {
            result.replace(startPos, from.length(), to);
            startPos += to.length();
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
        SizeType lastIndex = filename.FindLastIndex('.');

        if (lastIndex == String::NotFound)
        {
            return filename;
        }

        return filename.Substr(0, lastIndex);
    }

    static inline String GetExtension(const String& path)
    {
        Array<String> splitPath = path.Split('/', '\\');

        if (splitPath.Empty())
        {
            return "";
        }

        const String& filename = splitPath.Back();

        SizeType lastIndex = filename.FindLastIndex('.');

        if (lastIndex == String::NotFound)
        {
            return "";
        }

        return filename.Substr(lastIndex + 1);
    }

    static inline String ToPascalCase(const String& str, bool preserveCase = false)
    {
        Array<String> parts = str.Split('_', ' ', '-');

        for (SizeType i = 0; i < parts.Size(); i++)
        {
            String& part = parts[i];

            if (part.Empty())
            {
                continue;
            }

            if (!preserveCase)
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

    static inline String ToCamelCase(const String& str, bool preserveCase = false)
    {
        String pascalCase = ToPascalCase(str, preserveCase);
        if (pascalCase.Empty())
        {
            return pascalCase;
        }

        utf::Char32 firstChar = pascalCase.GetChar(0);
        if (firstChar >= 128)
        {
            // If the first character is not a ascii we just return the string as is - we only convert
            // the first character to lower case if it is an ascii character.
            return pascalCase;
        }

        String camelCase;
        camelCase.Append(std::tolower(int(firstChar)));
        camelCase.Append(pascalCase.Substr(1));

        return camelCase;
    }

    static inline String ToSnakeCase(const String& str)
    {
        String result;
        result.Reserve(str.Size() * 2);

        bool lastWasUpper = false;
        bool isFirstChar = true;

        for (SizeType i = 0; i < str.Size(); i++)
        {
            utf::Char32 ch = str.GetChar(i);

            if (ch == ' ' || ch == '-' || ch == '_')
            {
                if (!result.Empty() && result.Back() != '_')
                {
                    result.Append('_');
                }

                lastWasUpper = false;
                isFirstChar = false;
                continue;
            }

            if (ch >= 'A' && ch <= 'Z')
            {
                bool needsUnderscore = !isFirstChar && !result.Empty() && result.Back() != '_';

                if (needsUnderscore && lastWasUpper)
                {
                    if (i + 1 < str.Size())
                    {
                        utf::Char32 nextCh = str.GetChar(i + 1);
                        if (nextCh >= 'a' && nextCh <= 'z')
                        {
                            result.Append('_');
                        }
                    }
                }
                else if (needsUnderscore && !lastWasUpper)
                {
                    result.Append('_');
                }

                result.Append(std::tolower(ch));
                lastWasUpper = true;
            }
            else
            {
                result.Append(ch);
                lastWasUpper = false;
            }

            isFirstChar = false;
        }

        return result;
    }

    static inline bool Parse(const String& str, int* outValue)
    {
        *outValue = std::strtol(str.Data(), nullptr, 0);

        return true;
    }

    static inline bool Parse(const String& str, long* outValue)
    {
        *outValue = std::strtol(str.Data(), nullptr, 0);

        return true;
    }

    static inline bool Parse(const String& str, long long* outValue)
    {
        *outValue = std::strtoll(str.Data(), nullptr, 0);

        return true;
    }

    static inline bool Parse(const String& str, unsigned int* outValue)
    {
        unsigned int val = 0;
        const char* p = str.Data();

        while (*p)
        {
            val = (val << 1) + (val << 3) + *(p++) - 48;
        }

        *outValue = val;

        return true;
    }

    static inline bool Parse(const String& str, float* outValue)
    {
        *outValue = std::strtof(str.Data(), nullptr);

        return true;
    }

    static inline bool Parse(const String& str, double* outValue)
    {
        *outValue = std::strtod(str.Data(), nullptr);

        return true;
    }

    template <typename T>
    static inline bool Parse(const String& str, T* outValue)
    {
        std::istringstream ss(str.Data());
        T value;

        if (!(ss >> std::boolalpha >> value))
        {
            return false;
        }

        *outValue = value;

        return true;
    }

    template <typename T>
    static inline T Parse(const String& str, T valueOnError = T {})
    {
        T value = valueOnError;

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
