/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypClass.hpp>
#include <core/object/HypEnum.hpp>

#include <core/threading/ThreadID.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Class.hpp>

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
    AssertThrowMsg(m_is_initialized, "Cannot use GetClass() - HypClassRegistry instance not yet initialized");

    const auto it = m_registered_classes.Find(type_id);

    if (it == m_registered_classes.End()) {
        return nullptr;
    }

    return it->second;
}

const HypClass *HypClassRegistry::GetClass(WeakName type_name) const
{
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

const HypEnum *HypClassRegistry::GetEnum(TypeID type_id) const
{
    const HypClass *hyp_class = GetClass(type_id);

    if (!hyp_class || !(hyp_class->GetFlags() & HypClassFlags::ENUM_TYPE)) {
        return nullptr;
    }

    return static_cast<const HypEnum *>(hyp_class);
}

const HypEnum *HypClassRegistry::GetEnum(WeakName type_name) const
{
    const HypClass *hyp_class = GetClass(type_name);

    if (!hyp_class || !(hyp_class->GetFlags() & HypClassFlags::ENUM_TYPE)) {
        return nullptr;
    }

    return static_cast<const HypEnum *>(hyp_class);
}

void HypClassRegistry::RegisterClass(TypeID type_id, HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);

    if (type_id.IsDynamicType()) {
        Mutex::Guard guard(m_dynamic_classes_mutex);

        HYP_LOG(Object, Info, "Register dynamic class {}", hyp_class->GetName());

        m_dynamic_classes.Set(type_id, hyp_class);

        return;
    }

    AssertThrowMsg(!m_is_initialized, "Cannot register class - HypClassRegistry instance already initialized");

    HYP_LOG(Object, Info, "Register class {}", hyp_class->GetName());

    const auto it = m_registered_classes.Find(type_id);
    AssertThrowMsg(it == m_registered_classes.End(), "Class already registered for type: %s", *hyp_class->GetName());

    m_registered_classes.Set(type_id, hyp_class);
}

void HypClassRegistry::UnregisterClass(const HypClass *hyp_class)
{
    AssertThrowMsg(hyp_class->GetTypeID().IsDynamicType(), "Cannot unregister class - must be a dynamic HypClass to unregister");

    Mutex::Guard guard(m_dynamic_classes_mutex);

    auto it = m_dynamic_classes.FindIf([hyp_class](const Pair<TypeID, HypClass *> &item)
    {
        return item.second == hyp_class;
    });

    if (it == m_dynamic_classes.End()) {
        return;
    }

    HYP_LOG(Object, Info, "Unregister dynamic class {}", it->second->GetName());

    m_dynamic_classes.Erase(it);
}

void HypClassRegistry::ForEachClass(const ProcRef<IterationResult(const HypClass *)> &callback, bool include_dynamic_classes) const
{
    AssertThrowMsg(m_is_initialized, "Cannot use ForEachClass() - HypClassRegistry instance not yet initialized");

    for (const Pair<TypeID, HypClass *> &it : m_registered_classes) {
        if (callback(it.second) == IterationResult::STOP) {
            return;
        }
    }

    if (!include_dynamic_classes) {
        return;
    }

    Mutex::Guard guard(m_dynamic_classes_mutex);

    for (const Pair<TypeID, HypClass *> &it : m_dynamic_classes) {
        if (callback(it.second) == IterationResult::STOP) {
            return;
        }
    }
}

void HypClassRegistry::RegisterManagedClass(dotnet::Class *managed_class, const HypClass *hyp_class)
{
    AssertThrow(managed_class != nullptr);
    AssertThrow(hyp_class != nullptr);

    HYP_LOG(Object, Info, "Register managed class for {} on thread {}", hyp_class->GetName(), ThreadID::Current().GetName());

    Mutex::Guard guard(m_managed_classes_mutex);

    auto it = m_managed_classes.FindAs(hyp_class);
    AssertThrowMsg(it == m_managed_classes.End(), "Class %s already has a managed class registered for it", *hyp_class->GetName());

    if (managed_class->GetFlags() & ManagedClassFlags::STRUCT_TYPE) {
        if (const HypClassAttributeValue &size_attribute_value = hyp_class->GetAttribute("size"); size_attribute_value.IsValid()) {
            AssertThrowMsg(managed_class->GetSize() == size_attribute_value.GetInt(),
                "Expected value type managed class %s to have a size equal to %d but got %u",
                managed_class->GetName().Data(),
                size_attribute_value.GetInt(),
                managed_class->GetSize());
        } else {
            HYP_LOG(Object, Warning, "HypClass {} is missing \"Size\" attribute, cannot validate size of managed object matches", hyp_class->GetName());
        }

        // @TODO Validate fields all match
    }

    m_managed_classes.Insert(const_cast<HypClass *>(hyp_class), managed_class);
}

void HypClassRegistry::UnregisterManagedClass(dotnet::Class *managed_class)
{
    AssertThrow(managed_class != nullptr);

    HYP_LOG(Object, Info, "Unregister managed class {} on thread {}", managed_class->GetName(), ThreadID::Current().GetName());

    Mutex::Guard guard(m_managed_classes_mutex);

    for (auto it = m_managed_classes.Begin(); it != m_managed_classes.End();) {
        if (it->second == managed_class) {
            it = m_managed_classes.Erase(it);
        } else {
            ++it;
        }
    }
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
    AssertThrow(!m_is_initialized);

    // Have to initialize here because HypClass::Initialize will call GetClass() for parent classes.
    m_is_initialized = true;

    for (const Pair<TypeID, HypClass *> &it : m_registered_classes) {
        it.second->Initialize();
    }
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