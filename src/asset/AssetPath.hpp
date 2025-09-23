/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/Name.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/FormatFwd.hpp>

namespace hyperion {

HYP_STRUCT(Size = 8)
struct AssetPath
{
    HYP_FIELD(NoScriptBindings)
    Name* chain = nullptr;

    AssetPath() = default;

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

    HYP_METHOD(Property = "Chain", Serialize = true)
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

    HYP_METHOD(Property = "Chain", Serialize = true)
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
