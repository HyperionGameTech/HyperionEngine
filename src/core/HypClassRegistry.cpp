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

void HypClassRegistry::RegisterManagedClass(const HypClass *hyp_class, dotnet::Class *managed_class)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(managed_class != nullptr);

    Mutex::Guard guard(m_managed_classes_mutex);

    auto it = m_managed_classes.FindAs(hyp_class);
    AssertThrowMsg(it == m_managed_classes.End(), "Class %s already has a managed class registered for it", *hyp_class->GetName());

    m_managed_classes.Insert(const_cast<HypClass *>(hyp_class), managed_class);
}

dotnet::Class *HypClassRegistry::GetManagedClass(const HypClass *hyp_class) const
{
    if (!hyp_class) {
        return nullptr;
    }

    Mutex::Guard guard(m_managed_classes_mutex);

    auto it = m_managed_classes.FindAs(hyp_class);

    if (it == m_managed_classes.End()) {
        return nullptr;
    }

    return it->second;
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