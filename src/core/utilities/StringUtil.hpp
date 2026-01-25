/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>

namespace Hyperion {

namespace StringUtil {

static inline String Basename(const String& filepath)
{
    SizeType index0 = filepath.FindLastIndex('/');
    SizeType index1 = filepath.FindLastIndex('\\');

    if (index0 == String::NotFound && index1 == String::NotFound)
    {
        return filepath;
    }

    const SizeType lastIndex = (index0 == String::NotFound)
        ? index1
        : (index1 == String::NotFound ? index0 : (index0 > index1 ? index0 : index1));

    return filepath.Substr(lastIndex + 1);
}

static inline String BasePath(const String& filepath)
{
    SizeType index0 = filepath.FindLastIndex('/');
    SizeType index1 = filepath.FindLastIndex('\\');

    if (index0 == String::NotFound && index1 == String::NotFound)
    {
        return "";
    }

    const SizeType lastIndex = (index0 == String::NotFound)
        ? index1
        : (index1 == String::NotFound ? index0 : (index0 > index1 ? index0 : index1));

    return filepath.Substr(0, lastIndex);
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

template <int TStringType, class Predicate>
static inline containers::String<TStringType> TakeWhile(const containers::String<TStringType>& src, Predicate&& predicate)
{
    containers::String<TStringType> dst;
    dst.Reserve(src.Size());

    for (auto ch : src)
    {
        if (predicate(ch))
        {
            dst.Append(ch);
        }
        else
        {
            break;
        }
    }

    return dst;
}

static inline bool Parse(const String& str, int* outValue)
{
    *outValue = int(std::strtol(str.Data(), nullptr, 0));

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
    *outValue = unsigned(std::strtoul(str.Data(), nullptr, 0));

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

} // namespace StringUtil

} // namespace Hyperion
