/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/Name.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

#include <Types.hpp>

using namespace hyperion;

namespace hyperion {

#pragma region DynamicHypClassInstance

DynamicHypClassInstance::DynamicHypClassInstance(TypeID type_id, Name name, const HypClass* parent_class, dotnet::Class* class_ptr, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : HypClass(type_id, name, -1, 0, Name::Invalid(), attributes, flags, members)
{
    if (class_ptr != nullptr)
    {
        SetManagedClass(class_ptr->RefCountedPtrFromThis());
    }

    AssertThrowMsg(parent_class != nullptr, "Parent class cannot be null for DynamicHypClassInstance");

    m_parent = parent_class;
    m_parent_name = parent_class->GetName();

    if (!m_parent->CanCreateInstance())
    {
        HYP_LOG(Object, Error, "DynamicHypClassInstance: Will be unable to create an instance of class {} because parent class {} cannot create instance", *m_name, *m_parent->GetName());
    }
}

DynamicHypClassInstance::~DynamicHypClassInstance()
{
}

bool DynamicHypClassInstance::IsValid() const
{
    AssertThrow(m_parent != nullptr);

    return m_parent->IsValid();
}

HypClassAllocationMethod DynamicHypClassInstance::GetAllocationMethod() const
{
    AssertThrow(m_parent != nullptr);

    return m_parent->GetAllocationMethod();
}

SizeType DynamicHypClassInstance::GetSize() const
{
    AssertThrow(m_parent != nullptr);

    return m_parent->GetSize();
}

SizeType DynamicHypClassInstance::GetAlignment() const
{
    AssertThrow(m_parent != nullptr);

    return m_parent->GetAlignment();
}

bool DynamicHypClassInstance::GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const
{
    AssertThrow(m_parent != nullptr);

    const IHypObjectInitializer* initializer = m_parent->GetObjectInitializer(object_ptr);
    AssertThrow(initializer != nullptr);

    if (initializer->GetManagedObjectResource() == nullptr)
    {
        return false;
    }

    TResourceHandle<ManagedObjectResource> resource_handle(*initializer->GetManagedObjectResource());

    if (!resource_handle->GetManagedObject()->IsValid())
    {
        return false;
    }

    out_object_reference = resource_handle->GetManagedObject()->GetObjectReference();

    return true;
}

bool DynamicHypClassInstance::CanCreateInstance() const
{
    RC<dotnet::Class> managed_class = GetManagedClass();

    return m_parent->CanCreateInstance()
        && managed_class != nullptr
        && !(managed_class->GetFlags() & ManagedClassFlags::ABSTRACT);
}

bool DynamicHypClassInstance::ToHypData(ByteView memory, HypData& out_hyp_data) const
{
    AssertThrow(m_parent != nullptr);

    return m_parent->ToHypData(memory, out_hyp_data);
}

void DynamicHypClassInstance::FixupPointer(void* target, IHypObjectInitializer* new_initializer) const
{
    // Do nothing - FixupObjectInitializerPointer() will just keep iterating up the chain
}

void DynamicHypClassInstance::PostLoad_Internal(void* object_ptr) const
{
}

IHypObjectInitializer* DynamicHypClassInstance::GetObjectInitializer_Internal(void* object_ptr) const
{
    AssertThrow(m_parent != nullptr);

    return m_parent->GetObjectInitializer(object_ptr);
}

bool DynamicHypClassInstance::CreateInstance_Internal(HypData& out) const
{
    AssertThrow(m_parent != nullptr);

    RC<dotnet::Class> managed_class = GetManagedClass();
    AssertThrow(managed_class != nullptr);

    { // suppress default managed object creation - we will create it ourselves
        GlobalContextScope scope(HypObjectInitializerContext { this, HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

        {
            HypData value;

            if (!m_parent->CreateInstance(value, /* allow_abstract */ true))
            {
                return false;
            }

            AssertThrow(value.IsValid());

            if (m_parent->UseHandles())
            {
                AnyHandle handle = std::move(value.Get<AnyHandle>());
                AssertThrow(handle.IsValid());

                out = HypData(AnyHandle(this, handle.Get()));
            }
            else
            {
                out = std::move(value);
            }
        }
    }

    void* target_address = out.ToRef().GetPointer();
    AssertThrow(target_address != nullptr);

    IHypObjectInitializer* parent_initializer = m_parent->GetObjectInitializer(target_address);
    AssertThrow(parent_initializer != nullptr);

    DynamicHypObjectInitializer* new_initializer = new DynamicHypObjectInitializer(this, parent_initializer);
    FixupObjectInitializerPointer(target_address, new_initializer);

    ManagedObjectResource* managed_object_resource = AllocateResource<ManagedObjectResource>(HypObjectPtr(this, target_address), managed_class);
    new_initializer->SetManagedObjectResource(managed_object_resource);

    // Create the managed object
    managed_object_resource->IncRef();

    return true;
}

bool DynamicHypClassInstance::CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const
{
    AssertThrow(m_parent != nullptr);

    /// \todo: Find some way to support this.

    HYP_NOT_IMPLEMENTED();

    return false;
}

HashCode DynamicHypClassInstance::GetInstanceHashCode_Internal(ConstAnyRef ref) const
{
    HYP_NOT_IMPLEMENTED();
}

#pragma endregion DynamicHypClassInstance

} // namespace hyperion

extern "C"
{

#pragma region HypClass

    HYP_EXPORT const HypClass* HypClass_GetClassByName(const char* name)
    {
        if (!name)
        {
            return nullptr;
        }

        const WeakName weak_name(name);

        return HypClassRegistry::GetInstance().GetClass(weak_name);
    }

    HYP_EXPORT const HypClass* HypClass_GetClassByTypeID(const TypeID* type_id)
    {
        if (!type_id)
        {
            return nullptr;
        }

        return HypClassRegistry::GetInstance().GetClass(*type_id);
    }

    HYP_EXPORT const HypClass* HypClass_GetClassForManagedClass(const dotnet::Class* managed_class)
    {
        if (!managed_class)
        {
            return nullptr;
        }

        return managed_class->GetHypClass();
    }

    HYP_EXPORT const HypClass* HypClass_GetClassByTypeHash(dotnet::Assembly* assembly, int32 type_hash)
    {
        AssertThrow(assembly != nullptr);

        RC<dotnet::Class> managed_class = assembly->FindClassByTypeHash(type_hash);

        if (!managed_class)
        {
            return nullptr;
        }

        return managed_class->GetHypClass();
    }

    HYP_EXPORT void HypClass_GetName(const HypClass* hyp_class, Name* out_name)
    {
        if (!hyp_class || !out_name)
        {
            return;
        }

        *out_name = hyp_class->GetName();
    }

    HYP_EXPORT void HypClass_GetTypeID(const HypClass* hyp_class, TypeID* out_type_id)
    {
        if (!hyp_class || !out_type_id)
        {
            return;
        }

        *out_type_id = hyp_class->GetTypeID();
    }

    HYP_EXPORT uint32 HypClass_GetSize(const HypClass* hyp_class)
    {
        if (!hyp_class)
        {
            return 0;
        }

        return uint32(hyp_class->GetSize());
    }

    HYP_EXPORT uint32 HypClass_GetFlags(const HypClass* hyp_class)
    {
        if (!hyp_class)
        {
            return 0;
        }

        return uint32(hyp_class->GetFlags());
    }

    HYP_EXPORT uint8 HypClass_GetAllocationMethod(const HypClass* hyp_class)
    {
        if (!hyp_class)
        {
            return uint8(HypClassAllocationMethod::INVALID);
        }

        return uint8(hyp_class->GetAllocationMethod());
    }

    HYP_EXPORT uint32 HypClass_GetAttributes(const HypClass* hyp_class, const void** out_attributes)
    {
        if (!hyp_class || !out_attributes)
        {
            return 0;
        }

        if (hyp_class->GetAttributes().Empty())
        {
            return 0;
        }

        *out_attributes = hyp_class->GetAttributes().Begin();

        return (uint32)hyp_class->GetAttributes().Size();
    }

    HYP_EXPORT const HypClassAttribute* HypClass_GetAttribute(const HypClass* hyp_class, const char* name)
    {
        if (!hyp_class || !name)
        {
            return nullptr;
        }

        auto it = hyp_class->GetAttributes().Find(name);

        if (it == hyp_class->GetAttributes().End())
        {
            return nullptr;
        }

        return it;
    }

    HYP_EXPORT uint32 HypClass_GetProperties(const HypClass* hyp_class, const void** out_properties)
    {
        if (!hyp_class || !out_properties)
        {
            return 0;
        }

        if (hyp_class->GetProperties().Empty())
        {
            return 0;
        }

        *out_properties = hyp_class->GetProperties().Begin();

        return (uint32)hyp_class->GetProperties().Size();
    }

    HYP_EXPORT HypProperty* HypClass_GetProperty(const HypClass* hyp_class, const Name* name)
    {
        if (!hyp_class || !name)
        {
            return nullptr;
        }

        return hyp_class->GetProperty(*name);
    }

    HYP_EXPORT uint32 HypClass_GetMethods(const HypClass* hyp_class, const void** out_methods)
    {
        if (!hyp_class || !out_methods)
        {
            return 0;
        }

        if (hyp_class->GetMethods().Empty())
        {
            return 0;
        }

        *out_methods = hyp_class->GetMethods().Begin();

        return (uint32)hyp_class->GetMethods().Size();
    }

    HYP_EXPORT HypMethod* HypClass_GetMethod(const HypClass* hyp_class, const Name* name)
    {
        if (!hyp_class || !name)
        {
            return nullptr;
        }

        return hyp_class->GetMethod(*name);
    }

    HYP_EXPORT HypField* HypClass_GetField(const HypClass* hyp_class, const Name* name)
    {
        if (!hyp_class || !name)
        {
            return nullptr;
        }

        return hyp_class->GetField(*name);
    }

    HYP_EXPORT uint32 HypClass_GetFields(const HypClass* hyp_class, const void** out_fields)
    {
        if (!hyp_class || !out_fields)
        {
            return 0;
        }

        if (hyp_class->GetFields().Empty())
        {
            return 0;
        }

        *out_fields = hyp_class->GetFields().Begin();

        return (uint32)hyp_class->GetFields().Size();
    }

    HYP_EXPORT HypConstant* HypClass_GetConstant(const HypClass* hyp_class, const Name* name)
    {
        if (!hyp_class || !name)
        {
            return nullptr;
        }

        return hyp_class->GetConstant(*name);
    }

    HYP_EXPORT uint32 HypClass_GetConstants(const HypClass* hyp_class, const void** out_constants)
    {
        if (!hyp_class || !out_constants)
        {
            return 0;
        }

        if (hyp_class->GetConstants().Empty())
        {
            return 0;
        }

        *out_constants = hyp_class->GetConstants().Begin();

        return (uint32)hyp_class->GetConstants().Size();
    }

    HYP_EXPORT HypClass* HypClass_CreateDynamicHypClass(const TypeID* type_id, const char* name, const HypClass* parent_hyp_class)
    {
        AssertThrow(type_id != nullptr);
        AssertThrow(name != nullptr);
        AssertThrow(parent_hyp_class != nullptr);

        return new DynamicHypClassInstance(*type_id, CreateNameFromDynamicString(name), parent_hyp_class, nullptr, Span<const HypClassAttribute>(), HypClassFlags::CLASS_TYPE | HypClassFlags::DYNAMIC, Span<HypMember>());
    }

    HYP_EXPORT void HypClass_DestroyDynamicHypClass(DynamicHypClassInstance* hyp_class)
    {
        AssertThrow(hyp_class != nullptr);

        delete hyp_class;
    }

#pragma endregion HypClass

} // extern "C"