/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector2.hpp>
#include <core/math/Vector4.hpp>

#include <core/containers/Array.hpp>

#include <core/Types.hpp>

#include <algorithm>

namespace hyperion {

template <class AtlasElement>
struct AtlasPacker
{
    Vec2u atlasDimensions;
    Array<AtlasElement, DynamicAllocator> elements;
    Array<Pair<Vec2i, Vec2i>> freeSpaces;

    AtlasPacker()
        : atlasDimensions(Vec2u::One())
    {
    }

    AtlasPacker(const Vec2u& atlasDimensions);

    AtlasPacker(const AtlasPacker<AtlasElement>& other) = default;
    AtlasPacker<AtlasElement>& operator=(const AtlasPacker<AtlasElement>& other) = default;

    AtlasPacker(AtlasPacker<AtlasElement>&& other) = default;
    AtlasPacker<AtlasElement>& operator=(AtlasPacker<AtlasElement>&& other) = default;

    ~AtlasPacker() = default;

    /*! \brief Adds an element to the atlas, if it will fit.
     *  \param elementDimensions The dimensions of the element to add.
     *  \param outElement The element that was added, with its offset, scale and other properties set.
     *  \param outElementIndex The index that will be assigned when the element is added. Will be set to ~0u if not added.
     *  \param shrinkToFit If true, the dimensions element will be shrunk to fit the atlas if it doesn't fit. Aspect ratio will be maintained.
     *  \param downscaleLimit The lowest downscale ratio compared to `elementDimensions` to apply when shrinking the element to fit before giving up. (default: 0.25 = 25% of `elementDimensions`)
     *  \return True if the element was added successfully, false otherwise.
     */
    bool AddElement(const Vec2u& elementDimensions, AtlasElement& outElement, uint32& outElementIndex, bool shrinkToFit = true, float downscaleLimit = 0.25f);
    bool RemoveElement(const AtlasElement& element);

    void Clear();

    bool CalculateFitOffset(uint32 index, const Vec2u& dimensions, Vec2u& outOffset) const;
    void AddSkylineNode(uint32 beforeIndex, const Vec2u& dimensions, const Vec2u& offset);
    void MergeSkyline();
};

template <class AtlasElement>
AtlasPacker<AtlasElement>::AtlasPacker(const Vec2u& atlasDimensions)
    : atlasDimensions(atlasDimensions)
{
    freeSpaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlasDimensions.x), 0 });
}

template <class AtlasElement>
bool AtlasPacker<AtlasElement>::AddElement(const Vec2u& elementDimensions, AtlasElement& outElement, uint32& outElementIndex, bool shrinkToFit, float downscaleLimit)
{
    outElementIndex = ~0u;

    if (elementDimensions.x == 0 || elementDimensions.y == 0)
    {
        return false;
    }

    auto tryAddElementToSkyline = [this, &outElement, &outElementIndex](const Vec2u& dim) -> bool
    {
        int bestY = INT32_MAX;
        int bestX = -1;
        int bestIndex = -1;

        for (SizeType i = 0; i < freeSpaces.Size(); i++)
        {
            Vec2u offset;

            if (CalculateFitOffset(uint32(i), dim, offset))
            {
                if (int(offset.y) < bestY)
                {
                    bestX = int(offset.x);
                    bestY = int(offset.y);
                    bestIndex = int(i);
                }
            }
        }

        if (bestIndex != -1)
        {
            outElementIndex = uint32(elements.Size());

            outElement.offsetCoords = Vec2u { uint32(bestX), uint32(bestY) };
            outElement.offsetUv = Vec2f(outElement.offsetCoords) / Vec2f(atlasDimensions - 1);
            outElement.dimensions = dim;
            outElement.scale = Vec2f(dim) / Vec2f(atlasDimensions);

            elements.PushBack(outElement);

            AddSkylineNode(uint32(bestIndex), dim, Vec2u { uint32(bestX), uint32(bestY) });

            return true;
        }

        return false;
    };

    if (elementDimensions.x <= atlasDimensions.x && elementDimensions.y <= atlasDimensions.y)
    {
        if (tryAddElementToSkyline(elementDimensions))
        {
            return true;
        }
    }

    if (shrinkToFit)
    {
        // Maintain ratio, shrink the element dimensions to attempt to fit it into the atlas
        const float aspectRatio = float(elementDimensions.x) / float(elementDimensions.y);

        Vec2u newDimensions = elementDimensions;

        if (newDimensions.x > atlasDimensions.x || newDimensions.y > atlasDimensions.y)
        {
            if (newDimensions.x > atlasDimensions.x)
            {
                newDimensions.x = atlasDimensions.x;
                newDimensions.y = uint32(float(newDimensions.x) / aspectRatio);
            }

            if (newDimensions.y > atlasDimensions.y)
            {
                newDimensions.y = atlasDimensions.y;
                newDimensions.x = uint32(float(newDimensions.y) * aspectRatio);
            }
        }

        do
        {
            if (tryAddElementToSkyline(newDimensions))
            {
                return true;
            }

            // Reduce the dimensions by half each time until we reach the minimum downscale ratio
            newDimensions /= 2;
        }
        while ((Vec2f(newDimensions) / Vec2f(elementDimensions)).Length() >= downscaleLimit);
    }

    return false;
}

template <class AtlasElement>
bool AtlasPacker<AtlasElement>::RemoveElement(const AtlasElement& element)
{
    auto it = elements.Find(element);

    if (it == elements.End())
    {
        return false;
    }

    freeSpaces.EmplaceBack(Vec2i(element.offsetUv * Vec2f(atlasDimensions)), Vec2i(element.dimensions));

    std::sort(freeSpaces.Begin(), freeSpaces.End(), [](const auto& a, const auto& b)
        {
            return a.first.x < b.first.x;
        });

    MergeSkyline();

    elements.Erase(it);

    return true;
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::Clear()
{
    freeSpaces.Clear();
    elements.Clear();

    // Add the initial skyline node
    freeSpaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlasDimensions.x), 0 });
}

// Based on: https://jvernay.fr/en/blog/skyline-2d-packer/implementation/
template <class AtlasElement>
bool AtlasPacker<AtlasElement>::CalculateFitOffset(uint32 index, const Vec2u& dimensions, Vec2u& outOffset) const
{
    Vec2i spaceOffset = freeSpaces[index].first;
    Vec2i spaceDimensions = freeSpaces[index].second;

    const int x = spaceOffset.x;
    int y = spaceOffset.y + spaceDimensions.y;

    int remainingWidth = dimensions.x;

    if (x + dimensions.x > atlasDimensions.x)
    {
        return false;
    }

    for (uint32 i = index; i < freeSpaces.Size() && remainingWidth > 0; i++)
    {
        int nodeTopY = freeSpaces[i].first.y + freeSpaces[i].second.y;

        y = MathUtil::Max(y, nodeTopY);

        if (y + dimensions.y > atlasDimensions.y)
        {
            return false;
        }

        remainingWidth -= freeSpaces[i].second.x;
    }

    outOffset = Vec2u { uint32(x), uint32(y) };

    return true;
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::AddSkylineNode(uint32 beforeIndex, const Vec2u& dimensions, const Vec2u& offset)
{
    freeSpaces.Insert(freeSpaces.Begin() + beforeIndex, { Vec2i(offset), Vec2i(dimensions) });

    for (SizeType i = beforeIndex + 1; i < freeSpaces.Size();)
    {
        auto& spaceOffset = freeSpaces[i].first;
        auto& spaceDimensions = freeSpaces[i].second;

        if (spaceOffset.x < offset.x + dimensions.x)
        {
            int shrink = offset.x + dimensions.x - spaceOffset.x;

            spaceOffset.x += shrink;
            spaceDimensions.x -= shrink;

            if (spaceDimensions.x <= 0)
            {
                freeSpaces.EraseAt(i);
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    MergeSkyline();
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::MergeSkyline()
{
    // Should never happen as we always add at least one free space, but this will make debugging easier
    AssertDebug(freeSpaces.Any());

    for (SizeType i = 0; i < freeSpaces.Size() - 1;)
    {
        int y0 = freeSpaces[i].first.y + freeSpaces[i].second.y;
        int y1 = freeSpaces[i + 1].first.y + freeSpaces[i + 1].second.y;

        if (y0 == y1)
        {
            freeSpaces[i].second.x += freeSpaces[i + 1].second.x;
            freeSpaces.EraseAt(i + 1);
        }
        else
        {
            i++;
        }
    }
}

} // namespace hyperion
