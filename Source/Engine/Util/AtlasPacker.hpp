/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Types.hpp>

#include <algorithm>

namespace Hyperion {

template <class AtlasElement>
struct AtlasPacker
{
    Vec2u atlasDimensions;
    Array<Pair<Vec2i, Vec2i>> freeSpaces;
    
    Array<AtlasElement, DynamicAllocator> elements;
    Bitset freeElementSlots;

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
     *  \param outElement Out-reference, set to a pointer to the element that was added, with its offset, scale and other properties set.
               We use a pointer so the element can be mutated by the caller, assuming this call succeeded. Will be nullptr if it failed.
     *  \param outElementIndex The index that will be assigned when the element is added. Will be set to ~0u if not added.
     *  \param shrinkToFit If true, the dimensions element will be shrunk to fit the atlas if it doesn't fit. Aspect ratio will be maintained.
     *  \param downscaleLimit The lowest downscale ratio compared to `elementDimensions` to apply when shrinking the element to fit before giving up. (default: 0.25 = 25% of `elementDimensions`)
     *  \return True if the element was added successfully, false otherwise.
     */
    bool AddElement(const Vec2u& elementDimensions, AtlasElement*& outElement, uint32& outElementIndex, bool shrinkToFit = true, float downscaleLimit = 0.25f);
    bool RemoveElement(const AtlasElement& element);

    void Clear();

    bool CalculateFitOffset(uint32 index, const Vec2u& dimensions, Vec2u& outOffset) const;
    void AddSkylineNode(uint32 beforeIndex, const Vec2u& dimensions, const Vec2u& offset);
    void MergeSkyline();

private:
    void RebuildSkyline();
};

template <class AtlasElement>
AtlasPacker<AtlasElement>::AtlasPacker(const Vec2u& atlasDimensions)
    : atlasDimensions(atlasDimensions)
{
    freeSpaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlasDimensions.x), 0 });
}

template <class AtlasElement>
bool AtlasPacker<AtlasElement>::AddElement(
    const Vec2u& elementDimensions,
    AtlasElement*& outElement,
    uint32& outElementIndex,
    bool shrinkToFit,
    float downscaleLimit)
{
    outElementIndex = ~0u;
    outElement = nullptr;

    if (elementDimensions.x == 0 || elementDimensions.y == 0)
    {
        return false;
    }

    auto TryAddElementToSkyline = [this, &outElement, &outElementIndex](const Vec2u& dim) -> bool
    {
        int bestY = INT32_MAX;
        int bestX = -1;
        int bestIndex = -1;

        for (size_t i = 0; i < freeSpaces.Size(); i++)
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
            size_t freeIndex = freeElementSlots.FirstSetBitIndex();
            if (freeIndex != Bitset::NotFound)
            {
                // take the free slot, set it to zero now that we're using it.
                freeElementSlots.Set(freeIndex, false);

                outElementIndex = uint32(freeIndex);
            }
            else
            {
                elements.EmplaceBack();

                outElementIndex = uint32(elements.Size() - 1);
            }

            outElement = &elements[outElementIndex];

            *outElement = {};

            outElement->offsetCoords = Vec2u { uint32(bestX), uint32(bestY) };
            outElement->offsetUV = Vec2f(outElement->offsetCoords) / Vec2f(atlasDimensions);
            outElement->dimensions = dim;
            outElement->scale = Vec2f(dim) / Vec2f(atlasDimensions);

            AddSkylineNode(uint32(bestIndex), dim, Vec2u { uint32(bestX), uint32(bestY) });

            return true;
        }

        return false;
    };

    if (elementDimensions.x <= atlasDimensions.x && elementDimensions.y <= atlasDimensions.y)
    {
        if (TryAddElementToSkyline(elementDimensions))
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
            if (TryAddElementToSkyline(newDimensions))
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
    if (element.index >= elements.Size())
        return false;

    // Mark the slot as free and zero out the element (preserve indices for other elements).
    freeElementSlots.Set(element.index, true);
    elements[element.index] = {};

    RebuildSkyline();

    return true;
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::RebuildSkyline()
{
    Array<uint32> xBounds;
    xBounds.PushBack(0);
    xBounds.PushBack(atlasDimensions.x);

    for (uint32 i = 0; i < uint32(elements.Size()); i++)
    {
        if (freeElementSlots.Test(i))
            continue;

        const AtlasElement& e = elements[i];
        xBounds.PushBack(e.offsetCoords.x);

        const uint32 right = e.offsetCoords.x + e.dimensions.x;
        if (right <= atlasDimensions.x)
            xBounds.PushBack(right);
    }

    std::sort(xBounds.Begin(), xBounds.End());

    // Deduplicate
    for (size_t i = 1; i < xBounds.Size();)
    {
        if (xBounds[i] == xBounds[i - 1])
            xBounds.EraseAt(i);
        else
            ++i;
    }

    freeSpaces.Clear();

    for (uint32 seg = 0; seg + 1 < uint32(xBounds.Size()); seg++)
    {
        const int x0 = int(xBounds[seg]);
        const int x1 = int(xBounds[seg + 1]);

        int maxTop = 0;

        for (uint32 i = 0; i < uint32(elements.Size()); i++)
        {
            if (freeElementSlots.Test(i))
                continue;

            const AtlasElement& e = elements[i];
            if (int(e.offsetCoords.x) <= x0 && x0 < int(e.offsetCoords.x + e.dimensions.x))
            {
                const int top = int(e.offsetCoords.y) + int(e.dimensions.y);
                if (top > maxTop)
                    maxTop = top;
            }
        }

        freeSpaces.EmplaceBack(Vec2i { x0, 0 }, Vec2i { x1 - x0, maxTop });
    }

    MergeSkyline();
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::Clear()
{
    freeSpaces.Clear();
    // Add the initial skyline node
    freeSpaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlasDimensions.x), 0 });

    elements.Clear();

    freeElementSlots.Clear();
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

    for (size_t i = beforeIndex + 1; i < freeSpaces.Size();)
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

    for (size_t i = 0; i < freeSpaces.Size() - 1;)
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

} // namespace Hyperion
