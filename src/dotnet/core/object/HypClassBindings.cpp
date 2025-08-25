/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObject.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/Name.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

#include <core/Types.hpp>

using namespace hyperion;

namespace hyperion {

#pragma region DynamicHypClassInstance

#ifdef HYP_DOTNET
DynamicHypClassInstance::DynamicHypClassInstance(TypeId typeId, Name name, const HypClass* parentClass, dotnet::Class* classPtr, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : HypClass(typeId, name, -1, 0, Name::Invalid(), attributes, flags, members)
{
    if (classPtr != nullptr)
    {
        SetManagedClass(classPtr->RefCountedPtrFromThis());
    }

    Assert(parentClass != nullptr, "Parent class cannot be null for DynamicHypClassInstance");

    m_parent = parentClass;
    m_parentName = parentClass->GetName();

    if (!m_parent->CanCreateInstance())
    {
        HYP_LOG(Object, Error, "DynamicHypClassInstance: Will be unable to create an instance of class {} because parent class {} cannot create instance", *m_name, *m_parent->GetName());
    }

    m_size = m_parent->GetSize();
    m_alignment = m_parent->GetAlignment();
}
#endif

#ifdef HYP_SCRIPT
DynamicHypClassInstance::DynamicHypClassInstance(TypeId typeId, Name name, const HypClass* parentClass, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    : HypClass(typeId, name, -1, 0, Name::Invalid(), attributes, flags, members)
{
    Assert(parentClass != nullptr, "Parent class cannot be null for DynamicHypClassInstance");
}
#endif

DynamicHypClassInstance::~DynamicHypClassInstance()
{
}

bool DynamicHypClassInstance::IsValid() const
{
    Assert(m_parent != nullptr);

    return m_parent->IsValid();
}

HypClassAllocationMethod DynamicHypClassInstance::GetAllocationMethod() const
{
    Assert(m_parent != nullptr);

    return m_parent->GetAllocationMethod();
}

#ifdef HYP_DOTNET
bool DynamicHypClassInstance::GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const
{
    Assert(m_parent != nullptr);
    Assert(m_parent->UseHandles(), "Must be HypObjectBase type to call GetManagedObject");

    HypObjectBase* target = reinterpret_cast<HypObjectBase*>(const_cast<void*>(objectPtr));
    Assert(target != nullptr);

    if (target->GetScriptObjectResource() == nullptr)
    {
        return false;
    }

    TResourceHandle<ScriptObjectResource> resourceHandle(*target->GetScriptObjectResource());

    if (!resourceHandle->GetManagedObject()->IsValid())
    {
        return false;
    }

    outObjectReference = resourceHandle->GetManagedObject()->GetObjectReference();

    return true;
}
#endif

bool DynamicHypClassInstance::CanCreateInstance() const
{
#ifdef HYP_DOTNET
    RC<dotnet::Class> managedClass = GetManagedClass();

    return m_parent->CanCreateInstance()
        && managedClass != nullptr
        && !(managedClass->GetFlags() & ManagedClassFlags::ABSTRACT);
#else
    return false;
#endif
}

bool DynamicHypClassInstance::ToHypData(ByteView memory, HypData& outHypData) const
{
    Assert(m_parent != nullptr);

    return m_parent->ToHypData(memory, outHypData);
}

void DynamicHypClassInstance::PostLoad_Internal(void* objectPtr) const
{
}

bool DynamicHypClassInstance::CreateInstance_Internal(HypData& out) const
{
    Assert(m_parent != nullptr);

#ifdef HYP_DOTNET
    RC<dotnet::Class> managedClass = GetManagedClass();
    Assert(managedClass != nullptr);

    { // suppress default managed object creation - we will create it ourselves
        GlobalContextScope scope(HypObjectInitializerContext { this, HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION });

        {
            HypData value;

            if (!m_parent->CreateInstance(value, /* allowAbstract */ true))
            {
                return false;
            }

            Assert(value.IsValid());

            if (m_parent->UseHandles())
            {
                AnyHandle handle = std::move(value.Get<AnyHandle>());
                Assert(handle.IsValid());

                out = HypData(AnyHandle(this, handle.Get()));
            }
            else
            {
                out = std::move(value);
            }
        }
    }

    AssertDebug(m_parent->UseHandles());

    HypObjectBase* target = reinterpret_cast<HypObjectBase*>(out.ToRef().GetPointer());
    Assert(target != nullptr);

    ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>(HypObjectPtr(this, target), managedClass);
    AssertDebug(scriptObjectResource != nullptr);

    // keep it alive
    scriptObjectResource->IncRef();

    target->SetScriptObjectResource(scriptObjectResource);

    return true;
#else
    HYP_NOT_IMPLEMENTED();
#endif
}

bool DynamicHypClassInstance::CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const
{
    HYP_NOT_IMPLEMENTED();
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

        const WeakName weakName(name);

        return HypClassRegistry::GetInstance().GetClass(weakName);
    }

    HYP_EXPORT const HypClass* HypClass_GetClassByTypeId(const TypeId* typeId)
    {
        if (!typeId)
        {
            return nullptr;
        }

        return HypClassRegistry::GetInstance().GetClass(*typeId);
    }

    HYP_EXPORT const HypClass* HypClass_GetClassForManagedClass(const dotnet::Class* managedClass)
    {
        if (!managedClass)
        {
            return nullptr;
        }

        return managedClass->GetHypClass();
    }

    HYP_EXPORT const HypClass* HypClass_GetClassByTypeHash(dotnet::Assembly* assembly, int32 typeHash)
    {
        Assert(assembly != nullptr);

        RC<dotnet::Class> managedClass = assembly->FindClassByTypeHash(typeHash);

        if (!managedClass)
        {
            return nullptr;
        }

        return managedClass->GetHypClass();
    }

    HYP_EXPORT void HypClass_GetName(const HypClass* hypClass, Name* outName)
    {
        if (!hypClass || !outName)
        {
            return;
        }

        *outName = hypClass->GetName();
    }

    HYP_EXPORT void HypClass_GetTypeId(const HypClass* hypClass, TypeId* outTypeId)
    {
        if (!hypClass || !outTypeId)
        {
            return;
        }

        *outTypeId = hypClass->GetTypeId();
    }

    HYP_EXPORT uint32 HypClass_GetSize(const HypClass* hypClass)
    {
        if (!hypClass)
        {
            return 0;
        }

        return uint32(hypClass->GetSize());
    }

    HYP_EXPORT uint32 HypClass_GetFlags(const HypClass* hypClass)
    {
        if (!hypClass)
        {
            return 0;
        }

        return uint32(hypClass->GetFlags());
    }

    HYP_EXPORT uint8 HypClass_GetAllocationMethod(const HypClass* hypClass)
    {
        if (!hypClass)
        {
            return uint8(HypClassAllocationMethod::INVALID);
        }

        return uint8(hypClass->GetAllocationMethod());
    }

    HYP_EXPORT const HypClassAttribute* HypClass_GetAttribute(const HypClass* hypClass, const char* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        auto it = hypClass->GetAttributes().Find(name);

        if (it == hypClass->GetAttributes().End())
        {
            return nullptr;
        }

        return &*it;
    }

    HYP_EXPORT uint32 HypClass_GetProperties(const HypClass* hypClass, const void** outProperties)
    {
        if (!hypClass || !outProperties)
        {
            return 0;
        }

        if (hypClass->GetProperties().Empty())
        {
            return 0;
        }

        *outProperties = hypClass->GetProperties().Begin();

        return (uint32)hypClass->GetProperties().Size();
    }

    HYP_EXPORT HypProperty* HypClass_GetProperty(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetProperty(*name);
    }

    HYP_EXPORT uint32 HypClass_GetMethods(const HypClass* hypClass, const void** outMethods)
    {
        if (!hypClass || !outMethods)
        {
            return 0;
        }

        if (hypClass->GetMethods().Empty())
        {
            return 0;
        }

        *outMethods = hypClass->GetMethods().Begin();

        return (uint32)hypClass->GetMethods().Size();
    }

    HYP_EXPORT HypMethod* HypClass_GetMethod(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetMethod(*name);
    }

    HYP_EXPORT HypField* HypClass_GetField(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetField(*name);
    }

    HYP_EXPORT uint32 HypClass_GetFields(const HypClass* hypClass, const void** outFields)
    {
        if (!hypClass || !outFields)
        {
            return 0;
        }

        if (hypClass->GetFields().Empty())
        {
            return 0;
        }

        *outFields = hypClass->GetFields().Begin();

        return (uint32)hypClass->GetFields().Size();
    }

    HYP_EXPORT HypConstant* HypClass_GetConstant(const HypClass* hypClass, const Name* name)
    {
        if (!hypClass || !name)
        {
            return nullptr;
        }

        return hypClass->GetConstant(*name);
    }

    HYP_EXPORT uint32 HypClass_GetConstants(const HypClass* hypClass, const void** outConstants)
    {
        if (!hypClass || !outConstants)
        {
            return 0;
        }

        if (hypClass->GetConstants().Empty())
        {
            return 0;
        }

        *outConstants = hypClass->GetConstants().Begin();

        return (uint32)hypClass->GetConstants().Size();
    }

    HYP_EXPORT HypClass* HypClass_CreateDynamicHypClass(const TypeId* typeId, const char* name, const HypClass* parentHypClass)
    {
        Assert(typeId != nullptr);
        Assert(name != nullptr);
        Assert(parentHypClass != nullptr);

#ifdef HYP_DOTNET
        return new DynamicHypClassInstance(*typeId, CreateNameFromDynamicString(name), parentHypClass, nullptr, Span<const HypClassAttribute>(), HypClassFlags::CLASS_TYPE | HypClassFlags::DYNAMIC, Span<HypMember>());
#else
        return nullptr;
#endif
    }

    HYP_EXPORT void HypClass_DestroyDynamicHypClass(DynamicHypClassInstance* hypClass)
    {
        Assert(hypClass != nullptr);

        delete hypClass;
    }

#pragma endregion HypClass

} // extern "C"
