/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>

namespace Hyperion {

namespace StringUtil {

template <int StringType>
static inline auto Basename(const utilities::StringView<StringType>& filepath)
{
    using SV = utilities::StringView<StringType>;

    size_t index0 = filepath.FindLastIndex('/');
    size_t index1 = filepath.FindLastIndex('\\');

    if (index0 == SV::NotFound && index1 == SV::NotFound)
    {
        return filepath.Substr(0, SIZE_MAX); // whole string (view)
    }

    const size_t lastIndex = (index0 == SV::NotFound)
        ? index1
        : (index1 == SV::NotFound ? index0 : (index0 > index1 ? index0 : index1));

    return filepath.Substr(lastIndex + 1, SIZE_MAX);
}

/// String object overload. Delegates to String View overload.
template <int StringType, class AllocatorType>
static inline auto Basename(const containers::String<StringType, AllocatorType>& filepath)
{
    return Basename(utilities::StringView<StringType>(filepath));
}

/// Returns auto because it returns a view.
template <int StringType>
static inline auto BasePath(const utilities::StringView<StringType>& filepath)
{
    using SV = utilities::StringView<StringType>;

    size_t index0 = filepath.FindLastIndex('/');
    size_t index1 = filepath.FindLastIndex('\\');

    if (index0 == SV::NotFound
        && index1 == SV::NotFound)
    {
        return filepath.Substr(0, 0);
    }

    const size_t lastIndex = (index0 == SV::NotFound)
        ? index1
        : (index1 == SV::NotFound ? index0 : (index0 > index1 ? index0 : index1));

    return filepath.Substr(0, lastIndex);
}

/// String object overload. Delegates to String View overload.
template <int StringType, class AllocatorType>
static inline auto BasePath(const containers::String<StringType, AllocatorType>& filepath)
{
    return BasePath(utilities::StringView<StringType>(filepath));
}

template <int StringType, class AllocatorType>
static inline auto CanonicalizePath(const Array<containers::String<StringType, AllocatorType>>& original)
{
    using StringT = containers::String<StringType, AllocatorType>;

    Array<StringT> res;

    for (const StringT& str : original)
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

/// Returns auto because it returns a view.
template <int StringType>
static inline auto StripExtension(const utilities::StringView<StringType>& filepath)
{
    using SV = utilities::StringView<StringType>;

    size_t lastIndex = filepath.FindLastIndex('.');

    if (lastIndex == SV::NotFound)
    {
        return filepath.Substr(0, SIZE_MAX); // <-- to return a view and not confuse return types.
    }

    return filepath.Substr(0, lastIndex);
}

/// String object overload. Delegates to String View overload.
template <int StringType, class AllocatorType>
static inline auto StripExtension(const containers::String<StringType, AllocatorType>& filepath)
{
    return StripExtension(utilities::StringView<StringType>(filepath));
}

/// Splits into path components internally, so the result is returned as an owned String
/// rather than a view 
template <int StringType, class AllocatorType>
static inline auto GetExtension(const containers::String<StringType, AllocatorType>& path)
{
    using StringT = containers::String<StringType, AllocatorType>;

    Array<StringT> splitPath = path.Split('/', '\\');

    if (splitPath.Empty())
    {
        return StringT();
    }

    const StringT& fileName = splitPath.Back();

    size_t lastIndex = fileName.FindLastIndex('.');

    if (lastIndex == StringT::NotFound)
    {
        return StringT(); // <--- same deal as above for StripExtension(). This just returns "nothing"
    }

    return StringT(fileName.Substr(lastIndex + 1));
}

template <int StringType, class AllocatorType>
static inline auto ToPascalCase(const containers::String<StringType, AllocatorType>& str, bool preserveCase = false)
{
    using StringT = containers::String<StringType, AllocatorType>;

    Array<StringT> parts = str.Split('_', ' ', '-');

    for (size_t i = 0; i < parts.Size(); i++)
    {
        StringT& part = parts[i];

        if (part.Empty())
        {
            continue;
        }

        if (!preserveCase)
        {
            part = StringT(part.Substr(0, 1)).ToUpper() + StringT(part.Substr(1)).ToLower();
        }
        else
        {
            part = StringT(part.Substr(0, 1)).ToUpper() + StringT(part.Substr(1));
        }
    }

    return StringT::Join(parts, "");
}

template <int StringType, class AllocatorType>
static inline auto ToCamelCase(const containers::String<StringType, AllocatorType>& str, bool preserveCase = false)
{
    using StringT = containers::String<StringType, AllocatorType>;

    StringT pascalCase = ToPascalCase(str, preserveCase);
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

    StringT camelCase;
    camelCase.Append(std::tolower(int(firstChar)));
    camelCase.Append(pascalCase.Substr(1));

    return camelCase;
}

template <int StringType, class AllocatorType>
static inline auto ToSnakeCase(const containers::String<StringType, AllocatorType>& str)
{
    using StringT = containers::String<StringType, AllocatorType>;

    StringT result;
    result.Reserve(str.Size() * 2);

    bool lastWasUpper = false;
    bool isFirstChar = true;

    for (size_t i = 0; i < str.Size(); i++)
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

template <int StringType, class AllocatorType, class Predicate>
static inline auto TakeWhile(const containers::String<StringType, AllocatorType>& src, Predicate&& predicate)
{
    containers::String<StringType, AllocatorType> dst;
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

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, int8* outValue)
{
    *outValue = int8(strtol(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, int16* outValue)
{
    *outValue = int16(strtol(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, int32* outValue)
{
    *outValue = int32(strtol(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, int64* outValue)
{
    *outValue = int64(strtoll(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, uint8* outValue)
{
    *outValue = uint8(strtoul(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, uint16* outValue)
{
    *outValue = uint16(strtoul(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, uint32* outValue)
{
    *outValue = uint32(strtoul(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, uint64* outValue)
{
    *outValue = uint64(strtoull(str.Data(), nullptr, 0));

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, float* outValue)
{
    *outValue = strtof(str.Data(), nullptr);

    return true;
}

template <int StringType, class AllocatorType>
static inline bool Parse(const containers::String<StringType, AllocatorType>& str, double* outValue)
{
    *outValue = strtod(str.Data(), nullptr);

    return true;
}

/// StringView overload. Delegates to the String overload (Parse relies on the string being
/// null-terminated, which a view is not guaranteed to be).
template <int StringType, class T>
static inline bool Parse(const utilities::StringView<StringType>& str, T* outValue)
{
    return Parse(containers::String<StringType>(str), outValue);
}

template <class T, int StringType, class AllocatorType>
static inline T Parse(const containers::String<StringType, AllocatorType>& str, T valueOnError = T {})
{
    T value = valueOnError;

    Parse(str, &value);

    return value;
}

} // namespace StringUtil
} // namespace Hyperion
