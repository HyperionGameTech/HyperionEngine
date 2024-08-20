/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_ATTRIBUTE_HPP
#define HYPERION_CORE_HYP_CLASS_ATTRIBUTE_HPP

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Span.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

namespace hyperion {

struct HypClassAttribute
{
    ANSIStringView  name;
    ANSIStringView  value;

    constexpr HypClassAttribute() = default;
    constexpr HypClassAttribute(const ANSIStringView &name, const ANSIStringView &value)
        : name(name),
          value(value)
    {
    }

    constexpr HypClassAttribute(const HypClassAttribute &other)         = default;
    HypClassAttribute &operator=(const HypClassAttribute &other)        = default;

    constexpr HypClassAttribute(HypClassAttribute &&other) noexcept     = default;
    HypClassAttribute &operator=(HypClassAttribute &&other) noexcept    = default;

    static inline HashMap<String, String> ToHashMap(Span<HypClassAttribute> attributes)
    {
        if (!attributes) {
            return {};
        }

        HashMap<String, String> attributes_map;

        for (const HypClassAttribute &attribute : attributes) {
            attributes_map[attribute.name] = attribute.value;
        }

        return attributes_map;
    }
};

} // namespace hyperion

#endif