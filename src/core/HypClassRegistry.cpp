/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/HypClassRegistry.hpp>
#include <core/HypClass.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region HypClassRegistry

HypClassRegistry &HypClassRegistry::GetInstance()
{
    static HypClassRegistry instance;

    return instance;
}

const HypClass *HypClassRegistry::GetClass(TypeID type_id) const
{
    const auto it = m_registered_classes.Find(type_id);

    if (it == m_registered_classes.End()) {
        return nullptr;
    }

    return it->second;
}

const HypClass *HypClassRegistry::GetClass(WeakName type_name) const
{
    const auto it = m_registered_classes.FindIf([type_name](const Pair<TypeID, HypClass *> &item)
    {
        return item.second->GetName() == type_name;
    });

    if (it == m_registered_classes.End()) {
        return nullptr;
    }

    return it->second;
}

void HypClassRegistry::RegisterClass(TypeID type_id, HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);

    const auto it = m_registered_classes.Find(type_id);
    AssertThrowMsg(it == m_registered_classes.End(), "Class already registered for type: %s", *hyp_class->GetName());

    m_registered_classes.Set(type_id, hyp_class);
}

#pragma endregion HypClassRegistry

#pragma region HypClassRegistrationBase

namespace detail {
    
HypClassRegistrationBase::HypClassRegistrationBase(TypeID type_id, HypClass *hyp_class)
{
    HypClassRegistry::GetInstance().RegisterClass(type_id, hyp_class);
}

} // namespace detail

#pragma endregion HypClassRegistrationBase

} // namespace hyperion