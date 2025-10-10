/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <asset/AssetPath.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

AssetPath::AssetPath(const Array<Name>& names)
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

AssetPath::AssetPath(const UTF8StringView& path)
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

Array<Name> AssetPath::GetChain() const
{
    HYP_SCOPE;

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

void AssetPath::SetChain(const Array<Name>& names)
{
    HYP_SCOPE;

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

String AssetPath::MakeRelativePath(const AssetPath& from, const AssetPath& to)
{
    HYP_SCOPE;

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

AssetPath AssetPath::FromRelativePath(const AssetPath& from, const String& relativePath)
{
    HYP_SCOPE;

    if (!from.IsValid() || relativePath.Empty())
    {
        return AssetPath();
    }

    Array<Name> resultChain = from.GetChain();

    UTF8StringView substr = relativePath;
    SizeType index = 0;

    for (auto it = substr.Begin(); it != substr.End(); ++index)
    {
        if (*it == '/')
        {
            if (index != 0)
            {
                UTF8StringView segment = substr.Substr(0, index);

                if (segment == "..")
                {
                    if (resultChain.Empty())
                    {
                        HYP_LOG(Assets, Warning, "Attempted to navigate above root of asset path '{}'", from.ToString());
                        return AssetPath();
                    }

                    resultChain.PopBack();
                }
                else if (segment != ".")
                {
                    resultChain.PushBack(CreateNameFromDynamicString(segment));
                }
            }

            substr = substr.Substr(index + 1, SizeType(-1));
            it = substr.Begin();
            index = SizeType(-1); // will become 0 on next iteration
        }
        else
        {
            ++it;
        }
    }

    if (substr.Size() > 0)
    {
        if (substr == "..")
        {
            if (resultChain.Empty())
            {
                HYP_LOG(Assets, Warning, "Attempted to navigate above root of asset path '{}'", from.ToString());
                return AssetPath();
            }

            resultChain.PopBack();
        }
        else if (substr != ".")
        {
            resultChain.PushBack(CreateNameFromDynamicString(substr));
        }
    }

    return AssetPath(resultChain);
}

String AssetPath::ToString() const
{
    HYP_SCOPE;

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

} // namespace hyperion
