#ifndef HYPERION_CLASS_TRAITS_HPP
#define HYPERION_CLASS_TRAITS_HPP

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {
namespace utilities {

enum class ClassTraitFlags : uint8
{
    NONE = 0x0,
    DEFAULT_CONSTRUCTIBLE = 0x1,
    COPY_CONSTRUCTIBLE = 0x2,
    COPY_ASSIGNABLE = 0x4,
    MOVE_CONSTRUCTIBLE = 0x8,
    MOVE_ASSIGNABLE = 0x10
};

} // namespace utilities

using ClassTraitFlags = utilities::ClassTraitFlags;

HYP_MAKE_ENUM_FLAGS(ClassTraitFlags)

namespace utilities {

template <class... Types>
struct ClassTraits
{
    static constexpr bool default_constructible = (std::is_default_constructible_v<Types> && ...);
    static constexpr bool copy_constructible = (std::is_copy_constructible_v<Types> && ...);
    static constexpr bool copy_assignable = (std::is_copy_assignable_v<Types> && ...);
    static constexpr bool move_constructible = (std::is_move_constructible_v<Types> && ...);
    static constexpr bool move_assignable = (std::is_move_assignable_v<Types> && ...);

    static constexpr EnumFlags<ClassTraitFlags> value = (default_constructible ? ClassTraitFlags::DEFAULT_CONSTRUCTIBLE : ClassTraitFlags::NONE)
        | (copy_constructible ? ClassTraitFlags::COPY_CONSTRUCTIBLE : ClassTraitFlags::NONE)
        | (copy_assignable ? ClassTraitFlags::COPY_ASSIGNABLE : ClassTraitFlags::NONE)
        | (move_constructible ? ClassTraitFlags::MOVE_CONSTRUCTIBLE : ClassTraitFlags::NONE)
        | (move_assignable ? ClassTraitFlags::MOVE_ASSIGNABLE : ClassTraitFlags::NONE);
};

} // namespace utilities

template <class... Types>
using ClassTraits = utilities::ClassTraits;

} // namespace hyperion

#endif