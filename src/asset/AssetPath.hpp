/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObjectFwd.hpp>

#include <core/Name.hpp>

#include <core/math/MathUtil.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/FormatFwd.hpp>

namespace hyperion {

HYP_STRUCT(Size = 8)
struct HYP_API AssetPath
{
    HYP_STRUCT_BODY(AssetPath);

    HYP_PROPERTY(Value, &AssetPath::ToString, &AssetPath::Set)

    HYP_FIELD(NoScriptBindings, Transient)
    Name* chain = nullptr;

    AssetPath() = default;

    explicit AssetPath(const Array<Name>& names);
    explicit AssetPath(const UTF8StringView& path);

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

    HYP_METHOD()
    Name GetName() const
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
    Array<Name> GetChain() const;

    HYP_METHOD()
    void SetChain(const Array<Name>& names);

    static String MakeRelativePath(const AssetPath& from, const AssetPath& to);
    static AssetPath FromRelativePath(const AssetPath& from, const String& relativePath);

    HYP_METHOD()
    String ToString() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return ToString().GetHashCode();
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
