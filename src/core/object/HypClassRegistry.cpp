/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region HypClassRegistry

HypClassRegistry &HypClassRegistry::GetInstance()
{
    static HypClassRegistry instance;

    return instance;
}

HypClassRegistry::HypClassRegistry()
    : m_is_initialized(false)
{
}

HypClassRegistry::~HypClassRegistry()
{
}

const HypClass *HypClassRegistry::GetClass(TypeID type_id) const
{
    HYP_MT_CHECK_READ(m_data_race_detector);
    AssertThrowMsg(m_is_initialized, "Cannot use GetClass() - HypClassRegistry instance not yet initialized");

    const auto it = m_registered_classes.Find(type_id);

    if (it == m_registered_classes.End()) {
        return nullptr;
    }

    return it->second;
}

const HypClass *HypClassRegistry::GetClass(WeakName type_name) const
{
    HYP_MT_CHECK_READ(m_data_race_detector);
    AssertThrowMsg(m_is_initialized, "Cannot use GetClass() - HypClassRegistry instance not yet initialized");

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

    HYP_MT_CHECK_RW(m_data_race_detector);
    AssertThrowMsg(!m_is_initialized, "Cannot register class - HypClassRegistry instance already initialized");

    HYP_LOG(Object, LogLevel::INFO, "Register class {}", hyp_class->GetName());

    const auto it = m_registered_classes.Find(type_id);
    AssertThrowMsg(it == m_registered_classes.End(), "Class already registered for type: %s", *hyp_class->GetName());

    m_registered_classes.Set(type_id, hyp_class);
}

void HypClassRegistry::RegisterManagedClass(const HypClass *hyp_class, dotnet::Class *managed_class)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(managed_class != nullptr);

    HYP_LOG(Object, LogLevel::INFO, "Register managed class for {}", hyp_class->GetName());

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

void HypClassRegistry::Initialize()
{
    HYP_MT_CHECK_RW(m_data_race_detector);

    AssertThrow(!m_is_initialized);

    for (const Pair<TypeID, HypClass *> &it : m_registered_classes) {
        it.second->Initialize();
    }

    m_is_initialized = true;
}

#pragma endregion HypClassRegistry

#pragma region HypClassRegistrationBase

namespace detail {
    
HypClassRegistrationBase::HypClassRegistrationBase(TypeID type_id, HypClass *hyp_class)
{
    HYP_LOG(Object, LogLevel::DEBUG, "HypClassRegistrationBase constructor for type: {}", hyp_class->GetName());

    HypClassRegistry::GetInstance().RegisterClass(type_id, hyp_class);
}

} // namespace detail

#pragma endregion HypClassRegistrationBase

} // namespace hyperion