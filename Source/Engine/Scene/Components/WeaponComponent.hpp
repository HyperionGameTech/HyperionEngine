/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Weapon;

HYP_STRUCT(Component,
    Label = "Weapon Component")
struct WeaponComponent
{
    HYP_STRUCT_BODY(WeaponComponent);

    HYP_FIELD(Property = "Weapon", Title="Weapon Asset", Serialize, Editor)
    Handle<Weapon> weapon;
};

} // namespace Hyperion
