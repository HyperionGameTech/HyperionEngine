#pragma once
#include <Core/utilities/EnumFlags.hpp>

namespace Hyperion {
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
    static constexpr bool defaultConstructible = (std::is_default_constructible_v<Types> && ...);
    static constexpr bool copyConstructible = (std::is_copy_constructible_v<Types> && ...);
    static constexpr bool copyAssignable = (std::is_copy_assignable_v<Types> && ...);
    static constexpr bool moveConstructible = (std::is_move_constructible_v<Types> && ...);
    static constexpr bool moveAssignable = (std::is_move_assignable_v<Types> && ...);

    static constexpr EnumFlags<ClassTraitFlags> value = (defaultConstructible ? ClassTraitFlags::DEFAULT_CONSTRUCTIBLE : ClassTraitFlags::NONE)
        | (copyConstructible ? ClassTraitFlags::COPY_CONSTRUCTIBLE : ClassTraitFlags::NONE)
        | (copyAssignable ? ClassTraitFlags::COPY_ASSIGNABLE : ClassTraitFlags::NONE)
        | (moveConstructible ? ClassTraitFlags::MOVE_CONSTRUCTIBLE : ClassTraitFlags::NONE)
        | (moveAssignable ? ClassTraitFlags::MOVE_ASSIGNABLE : ClassTraitFlags::NONE);
};

} // namespace utilities

using utilities::ClassTraits;

} // namespace Hyperion
