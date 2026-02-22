/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/AssetPath.hpp>

#include <AssetPath.generated.inl>

namespace Hyperion {

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
                UTF8StringView segment = substr.Substr(0, index);

                // Normalize the path by handling ".." and "." segments
                if (segment == "..")
                {
                    if (names.Any())
                    {
                        names.PopBack();
                    }
                    else
                    {
                        HYP_LOG(Assets, Warning, "Attempted to navigate above root when parsing asset path '{}'", path);
                    }
                }
                else if (segment != ".")
                {
                    names.PushBack(CreateNameFromDynamicString(segment));
                }
            }

            substr = substr.Substr(index + 1, SizeType(-1));
            index = SizeType(-1); // will become 0 on next iteration
        }
    }

    if (substr.Size() > 0)
    {
        // Handle the last segment
        if (substr == "..")
        {
            if (names.Any())
            {
                names.PopBack();
            }
            else
            {
                HYP_LOG(Assets, Warning, "Attempted to navigate above root when parsing asset path '{}'", path);
            }
        }
        else if (substr != ".")
        {
            names.PushBack(CreateNameFromDynamicString(substr));
        }
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

    HYP_LOG(Assets, Debug, "MakeRelativePath: from='{}' to='{}'", from.ToString(), to.ToString());

    // Find common prefix length
    const SizeType minSize = MathUtil::Min(fromChain.Size(), toChain.Size());

    SizeType commonPrefixLen = 0;

    for (SizeType i = 0; i < minSize; i++)
    {
        if (fromChain[i] != toChain[i])
        {
            break;
        }

        commonPrefixLen++;
    }

    // If the paths are identical, return "."
    if (commonPrefixLen == fromChain.Size() && commonPrefixLen == toChain.Size())
    {
        return String(".");
    }

    // If 'to' is a direct child/descendant of 'from', just return the relative portion
    if (commonPrefixLen == fromChain.Size())
    {
        String result;

        for (SizeType i = commonPrefixLen; i < toChain.Size(); i++)
        {
            if (result.Length() > 0)
            {
                result.Append('/');
            }

            result.Append(*toChain[i]);
        }

        return result;
    }

    String result;

    // Add ".." for each segment we need to go up from 'from' to reach the common ancestor
    // We need to go up (fromChain.Size() - commonPrefixLen) levels
    const SizeType levelsUp = fromChain.Size() - commonPrefixLen;

    for (SizeType i = 0; i < levelsUp; i++)
    {
        if (result.Length() > 0)
        {
            result.Append('/');
        }

        result.Append("..");
    }

    // Add remaining elements from 'to' path after the common prefix
    for (SizeType i = commonPrefixLen; i < toChain.Size(); i++)
    {
        if (result.Length() > 0)
        {
            result.Append('/');
        }

        result.Append(*toChain[i]);
    }

    HYP_LOG(Assets, Debug, "MakeRelativePath result: '{}'", result);

    return result;
}

AssetPath AssetPath::FromRelativePath(const AssetPath& from, const String& relativePath)
{
    HYP_SCOPE;

    HYP_LOG(Assets, Debug, "FromRelativePath: from='{}' relativePath='{}'", from.ToString(), relativePath);

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

    AssetPath result(resultChain);

    HYP_LOG(Assets, Debug, "FromRelativePath result: '{}'", result.ToString());

    return result;
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

} // namespace Hyperion
