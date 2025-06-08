/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ATLAS_PACKER_HPP
#define HYPERION_ATLAS_PACKER_HPP

#include <core/math/Vector2.hpp>
#include <core/math/Vector4.hpp>

#include <core/containers/Array.hpp>

#include <Types.hpp>

#include <algorithm>

namespace hyperion {

template <class AtlasElement>
struct AtlasPacker
{
    Vec2u atlas_dimensions;
    Array<AtlasElement, DynamicAllocator> elements;
    Array<Pair<Vec2i, Vec2i>> free_spaces;

    AtlasPacker()
        : atlas_dimensions(Vec2u::One())
    {
    }

    AtlasPacker(const Vec2u& atlas_dimensions);

    AtlasPacker(const AtlasPacker<AtlasElement>& other) = default;
    AtlasPacker<AtlasElement>& operator=(const AtlasPacker<AtlasElement>& other) = default;

    AtlasPacker(AtlasPacker<AtlasElement>&& other) = default;
    AtlasPacker<AtlasElement>& operator=(AtlasPacker<AtlasElement>&& other) = default;

    ~AtlasPacker() = default;

    /*! \brief Adds an element to the atlas, if it will fit.
     *  \param element_dimensions The dimensions of the element to add.
     *  \param out_element The element that was added, with its offset, scale and other properties set.
     *  \param shrink_to_fit If true, the dimensions element will be shrunk to fit the atlas if it doesn't fit. Aspect ratio will be maintained.
     *  \param downscale_limit The lowest downscale ratio compared to `element_dimensions` to apply when shrinking the element to fit before giving up. (default: 0.25 = 25% of `element_dimensions`)
     *  \return True if the element was added successfully, false otherwise.
     */
    bool AddElement(const Vec2u& element_dimensions, AtlasElement& out_element, bool shrink_to_fit = true, float downscale_limit = 0.25f);
    bool RemoveElement(const AtlasElement& element);

    void Clear();

    bool CalculateFitOffset(uint32 index, const Vec2u& dimensions, Vec2u& out_offset) const;
    void AddSkylineNode(uint32 before_index, const Vec2u& dimensions, const Vec2u& offset);
    void MergeSkyline();
};

template <class AtlasElement>
AtlasPacker<AtlasElement>::AtlasPacker(const Vec2u& atlas_dimensions)
    : atlas_dimensions(atlas_dimensions)
{
    free_spaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlas_dimensions.x), 0 });
}

template <class AtlasElement>
bool AtlasPacker<AtlasElement>::AddElement(const Vec2u& element_dimensions, AtlasElement& out_element, bool shrink_to_fit, float downscale_limit)
{
    if (element_dimensions.x == 0 || element_dimensions.y == 0)
    {
        return false;
    }

    auto try_add_element_to_skyline = [this, &out_element](const Vec2u& dim) -> bool
    {
        int best_y = INT32_MAX;
        int best_x = -1;
        int best_index = -1;

        for (SizeType i = 0; i < free_spaces.Size(); i++)
        {
            Vec2u offset;

            if (CalculateFitOffset(uint32(i), dim, offset))
            {
                if (int(offset.y) < best_y)
                {
                    best_x = int(offset.x);
                    best_y = int(offset.y);
                    best_index = int(i);
                }
            }
        }

        if (best_index != -1)
        {
            out_element.index = elements.Size();
            out_element.offset_coords = Vec2u { uint32(best_x), uint32(best_y) };
            out_element.offset_uv = Vec2f(out_element.offset_coords) / Vec2f(atlas_dimensions - 1);
            out_element.dimensions = dim;
            out_element.scale = Vec2f(dim) / Vec2f(atlas_dimensions);

            elements.PushBack(out_element);

            AddSkylineNode(uint32(best_index), dim, Vec2u { uint32(best_x), uint32(best_y) });

            return true;
        }

        return false;
    };

    if (element_dimensions.x <= atlas_dimensions.x && element_dimensions.y <= atlas_dimensions.y)
    {
        if (try_add_element_to_skyline(element_dimensions))
        {
            return true;
        }
    }

    if (shrink_to_fit)
    {
        // Maintain ratio, shrink the element dimensions to attempt to fit it into the atlas
        const float aspect_ratio = float(element_dimensions.x) / float(element_dimensions.y);

        Vec2u new_dimensions = element_dimensions;

        if (new_dimensions.x > atlas_dimensions.x || new_dimensions.y > atlas_dimensions.y)
        {
            if (new_dimensions.x > atlas_dimensions.x)
            {
                new_dimensions.x = atlas_dimensions.x;
                new_dimensions.y = uint32(float(new_dimensions.x) / aspect_ratio);
            }

            if (new_dimensions.y > atlas_dimensions.y)
            {
                new_dimensions.y = atlas_dimensions.y;
                new_dimensions.x = uint32(float(new_dimensions.y) * aspect_ratio);
            }
        }

        do
        {
            if (try_add_element_to_skyline(new_dimensions))
            {
                return true;
            }

            // Reduce the dimensions by half each time until we reach the minimum downscale ratio
            new_dimensions /= 2;
        }
        while ((Vec2f(new_dimensions) / Vec2f(element_dimensions)).Length() >= downscale_limit);
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

    free_spaces.EmplaceBack(Vec2i(element.offset_uv * Vec2f(atlas_dimensions)), Vec2i(element.dimensions));

    std::sort(free_spaces.Begin(), free_spaces.End(), [](const auto& a, const auto& b)
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
    free_spaces.Clear();
    elements.Clear();

    // Add the initial skyline node
    free_spaces.EmplaceBack(Vec2i::Zero(), Vec2i { int(atlas_dimensions.x), 0 });
}

// Based on: https://jvernay.fr/en/blog/skyline-2d-packer/implementation/
template <class AtlasElement>
bool AtlasPacker<AtlasElement>::CalculateFitOffset(uint32 index, const Vec2u& dimensions, Vec2u& out_offset) const
{
    Vec2i space_offset = free_spaces[index].first;
    Vec2i space_dimensions = free_spaces[index].second;

    const int x = space_offset.x;
    int y = space_offset.y + space_dimensions.y;

    int remaining_width = dimensions.x;

    if (x + dimensions.x > atlas_dimensions.x)
    {
        return false;
    }

    for (uint32 i = index; i < free_spaces.Size() && remaining_width > 0; i++)
    {
        int node_top_y = free_spaces[i].first.y + free_spaces[i].second.y;

        y = MathUtil::Max(y, node_top_y);

        if (y + dimensions.y > atlas_dimensions.y)
        {
            return false;
        }

        remaining_width -= free_spaces[i].second.x;
    }

    out_offset = Vec2u { uint32(x), uint32(y) };

    return true;
}

template <class AtlasElement>
void AtlasPacker<AtlasElement>::AddSkylineNode(uint32 before_index, const Vec2u& dimensions, const Vec2u& offset)
{
    free_spaces.Insert(free_spaces.Begin() + before_index, { Vec2i(offset), Vec2i(dimensions) });

    for (SizeType i = before_index + 1; i < free_spaces.Size();)
    {
        auto& space_offset = free_spaces[i].first;
        auto& space_dimensions = free_spaces[i].second;

        if (space_offset.x < offset.x + dimensions.x)
        {
            int shrink = offset.x + dimensions.x - space_offset.x;

            space_offset.x += shrink;
            space_dimensions.x -= shrink;

            if (space_dimensions.x <= 0)
            {
                free_spaces.EraseAt(i);
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
    AssertThrow(free_spaces.Any());

    for (SizeType i = 0; i < free_spaces.Size() - 1;)
    {
        int y0 = free_spaces[i].first.y + free_spaces[i].second.y;
        int y1 = free_spaces[i + 1].first.y + free_spaces[i + 1].second.y;

        if (y0 == y1)
        {
            free_spaces[i].second.x += free_spaces[i + 1].second.x;
            free_spaces.EraseAt(i + 1);
        }
        else
        {
            i++;
        }
    }
}

} // namespace hyperion

#endif
