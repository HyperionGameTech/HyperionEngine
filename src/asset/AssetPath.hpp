/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/Name.hpp>

#include <core/math/MathUtil.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/FormatFwd.hpp>

namespace hyperion {

HYP_STRUCT(Size = 8)
struct AssetPath
{
    HYP_PROPERTY(Value, &AssetPath::ToString, &AssetPath::Set)

    HYP_FIELD(NoScriptBindings, Transient)
    Name* chain = nullptr;

    AssetPath() = default;

    explicit AssetPath(const Array<Name>& names)
        : chain(nullptr)
    {
        if (names.Empty())
        {
            return;
        }

        chain = new Name[names.Size() + 1];

        for (SizeType i = 0; i < names.Size(); i++)
        {
            chain[i] = names[i];
        }
    }

    explicit AssetPath(const UTF8StringView& path)
        : chain(nullptr)
    {
        if (!path)
        {
            return;
        }

        Array<Name> names;

        SizeType index = 0;

        UTF8StringView substr = path;

        for (auto it = substr.Begin(); it != substr.End(); ++it, ++index)
        {
            if (*it == '/')
            {
                if (index != 0)
                {
                    names.PushBack(CreateNameFromDynamicString(substr.Substr(0, index)));
                }

                substr = substr.Substr(index + 1, SizeType(-1));
                index = SizeType(-1); // will become 0 on next iteration
            }
        }

        if (substr.Size() > 0)
        {
            names.PushBack(CreateNameFromDynamicString(substr));
        }

        if (names.Empty())
        {
            return;
        }

        chain = new Name[names.Size() + 1];

        for (SizeType i = 0; i < names.Size(); i++)
        {
            chain[i] = names[i];
        }
    }

    AssetPath(const AssetPath& other)
    {
        if (other.chain)
        {
            uint32 count = 0;

            Name* curr = other.chain;

            while (curr->IsValid())
            {
                ++count;
                ++curr;
            }

            chain = new Name[count + 1];

            for (uint32 i = 0; i < count; i++)
            {
                chain[i] = other.chain[i];
            }

            chain[count] = Name::Invalid();
        }
    }

    AssetPath& operator=(const AssetPath& other)
    {
        if (this == &other)
        {
            return *this;
        }

        if (chain)
        {
            delete[] chain;
        }

        if (other.chain)
        {
            uint32 count = 0;

            Name* curr = other.chain;

            while (curr->IsValid())
            {
                ++count;
                ++curr;
            }

            chain = new Name[count + 1];

            for (uint32 i = 0; i < count; i++)
            {
                chain[i] = other.chain[i];
            }

            chain[count] = Name::Invalid();
        }
        else
        {
            chain = nullptr;
        }

        return *this;
    }

    AssetPath(AssetPath&& other) noexcept
        : chain(other.chain)
    {
        other.chain = nullptr;
    }

    AssetPath& operator=(AssetPath&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (chain)
        {
            delete[] chain;
        }

        chain = other.chain;
        other.chain = nullptr;

        return *this;
    }

    ~AssetPath()
    {
        if (chain)
        {
            delete[] chain;
        }
    }

    bool operator==(const AssetPath& other) const
    {
        if (chain == nullptr && other.chain == nullptr)
        {
            return true;
        }

        if (chain == nullptr || other.chain == nullptr)
        {
            return false;
        }

        Name* a = chain;
        Name* b = other.chain;

        while (a->IsValid() && b->IsValid())
        {
            if (*a != *b)
            {
                return false;
            }

            ++a;
            ++b;
        }

        return a->IsValid() == b->IsValid();
    }

    HYP_FORCE_INLINE bool operator!=(const AssetPath& other) const
    {
        return !(*this == other);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsValid() const
    {
        return chain && chain[0].IsValid();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        // name is the last element in the chain
        if (!chain)
        {
            return Name::Invalid();
        }

        Name* curr = chain;
        Name* last = curr;

        while (curr->IsValid())
        {
            last = curr;
            ++curr;
        }

        return *last;
    }

    void Set(const String& path)
    {
        *this = AssetPath(path);
    }

    HYP_METHOD()
    String ToString() const
    {
        if (!chain)
        {
            return String::empty;
        }

        String result;

        Name* curr = chain;

        while (curr->IsValid())
        {
            if (chain != curr)
            {
                result.Append("/");
            }

            result.Append(curr->LookupString());

            ++curr;
        }

        return result;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return ToString().GetHashCode();
    }

    HYP_METHOD()
    Array<Name> GetChain() const
    {
        Array<Name> result;

        if (chain)
        {
            Name* curr = chain;

            while (curr->IsValid())
            {
                result.PushBack(*curr);
                ++curr;
            }
        }

        return result;
    }

    HYP_METHOD()
    void SetChain(const Array<Name>& names)
    {
        if (chain)
        {
            delete[] chain;
            chain = nullptr;
        }

        if (names.Empty())
        {
            return;
        }

        chain = new Name[names.Size() + 1];

        for (SizeType i = 0; i < names.Size(); i++)
        {
            chain[i] = names[i];
        }

        chain[names.Size()] = Name::Invalid();
    }

    static String MakeRelativePath(const AssetPath& from, const AssetPath& to)
    {
        if (!from.IsValid() || !to.IsValid())
        {
            return String::empty;
        }

        Array<Name> fromChain = from.GetChain();
        Array<Name> toChain = to.GetChain();

        const SizeType minSize = MathUtil::Min(fromChain.Size(), toChain.Size());

        SizeType len = 0;

        for (SizeType i = 0; i < minSize; i++)
        {
            if (fromChain[i] != toChain[i])
            {
                break;
            }

            len++;
        }

        String result;

        // add ".." for each remaining element in from_chain
        for (SizeType i = len; i < fromChain.Size(); i++)
        {
            if (result.Length() > 0)
            {
                result.Append('/');
            }

            result.Append("..");
        }

        // add remaining elements in to_chain
        for (SizeType i = len; i < toChain.Size(); i++)
        {
            if (result.Length() > 0)
            {
                result.Append('/');
            }

            result.Append(*toChain[i]);
        }

        return result;
    }

    static AssetPath FromRelativePath(const AssetPath& from, const String& relativePath)
    {
        if (!from.IsValid() || relativePath.Empty())
        {
            return AssetPath();
        }

        Array<Name> resultChain = from.GetChain();

        UTF8StringView substr = relativePath;
        SizeType index = 0;

        for (auto it = substr.Begin(); it != substr.End(); ++it, ++index)
        {
            if (*it == '/')
            {
                if (index != 0)
                {
                    UTF8StringView segment = substr.Substr(0, index);

                    if (segment == "..")
                    {
                        if (!resultChain.Empty())
                        {
                            resultChain.PopBack();
                        }
                    }
                    else if (segment != ".")
                    {
                        resultChain.PushBack(CreateNameFromDynamicString(segment));
                    }
                }

                substr = substr.Substr(index + 1, SizeType(-1));
                index = SizeType(-1); // will become 0 on next iteration
            }
        }

        if (substr.Size() > 0)
        {
            if (substr == "..")
            {
                if (!resultChain.Empty())
                {
                    resultChain.PopBack();
                }
            }
            else if (substr != ".")
            {
                resultChain.PushBack(CreateNameFromDynamicString(substr));
            }
        }

        return AssetPath(resultChain);
    }
};

// Formatter for AssetPath
namespace utilities {

template <class StringType>
struct Formatter<StringType, AssetPath>
{
    auto operator()(const AssetPath& value) const
    {
        return value.ToString();
    }
};

} // namespace utilities

} // namespace hyperion
