/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <Core/reflection/Object.hpp>
#include <Core/reflection/ObjectPool.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <Core/reflection/Class.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/utilities/GlobalContext.hpp>

#include <Core/containers/Stack.hpp>

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)

#ifdef HYP_DOTNET
#include <dotnet/ManagedClass.hpp>
#include <dotnet/ManagedObject.hpp>
#endif

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

#include <scripting/ScriptObjectResource.hpp>

#endif

namespace Hyperion {

#ifdef HYP_TOOL
const Class* g_clsObjectBase = nullptr;
#else
HYP_API extern const Class* g_clsObjectBase;
#endif

HYP_API extern Pool* g_objectPool;

#pragma region ObjectInitializerGuardBase

ObjectInitializerGuardBase::ObjectInitializerGuardBase(TypedObjPtr ptr)
    : ptr(ptr)
{
    AssertDebug(ptr.IsValid());

    count = 0;

    AssertDebug(ptr.GetClass()->UseHandles());

    ObjectBase* target = reinterpret_cast<ObjectBase*>(ptr.GetPointer());
    AssertDebug(target != nullptr, "ObjectInitializerGuardBase: TypedObjPtr is not valid!");

    // Push NONE to prevent our current flags from polluting allocations that happen in the constructor
    PushGlobalContext(ObjectInitializerContext {
        .cls = ptr.GetClass(),
        .flags = ObjectInitializerFlags::NONE });
}

ObjectInitializerGuardBase::~ObjectInitializerGuardBase()
{
    PopGlobalContext<ObjectInitializerContext>();

    if (!ptr.IsValid())
    {
        return;
    }

    const Class* cls = ptr.GetClass();
    AssertDebug(cls->UseHandles()); // check is ObjectBase

    ObjectBase* target = reinterpret_cast<ObjectBase*>(ptr.GetPointer());
    AssertDebug(target->GetObjectHeader_Internal()->GetRefCountStrong() == 1);

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    AssertDebug(target->GetScriptObjectResource() == nullptr);

    if (!(ptr.GetClass()->GetFlags() & ClassFlags::NO_SCRIPT_BINDINGS))
    {
        ObjectInitializerContext* context = GetGlobalContext<ObjectInitializerContext>();

        if ((!context || !(context->flags & ObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)) && !cls->IsAbstract())
        {
            ScriptObjectResource* scriptObjectResource = target->GetScriptObjectResource();

#ifdef HYP_DOTNET
            if (RC<dotnet::ManagedClass> managedClass = cls->GetManagedClass())
            {
                if (!scriptObjectResource)
                {
                    scriptObjectResource = new ScriptObjectResource(target, managedClass);

                    target->SetScriptObjectResource(scriptObjectResource);
                }
                else
                {
                    scriptObjectResource->SetScriptObjectData_DotNet(ScriptObjectData_DotNet {
                        .objectPtr = nullptr,
                        .managedClass = managedClass });
                }

                scriptObjectResource->AddReader();
            }
            else
            {
                HYP_LOG(Core, Debug, "Class '%s' has no .NET class associated with it\n", *cls->GetName());
            }
#endif

#ifdef HYP_SCRIPT
            BoxedValue obj;

            if (!scriptObjectResource)
            {
                scriptObjectResource = new ScriptObjectResource((Script_Instance*)nullptr, std::move(obj));

                target->SetScriptObjectResource(scriptObjectResource);
            }
            else
            {
                scriptObjectResource->SetScriptObjectData_HypScript(ScriptObjectData_HypScript {
                    .instance = nullptr,
                    .obj = std::move(obj)
                });
            }
#endif
        }
    }
#endif
}

#pragma endregion ObjectInitializerGuardBase

#pragma region ObjectHeader

ObjectBase* ObjectHeader::GetObjectPointer(ObjectHeader* header)
{
    AssertDebug(header != nullptr);
    AssertDebug(header->cls != nullptr);

    // get pointer to object
    ObjectBase* ptr = reinterpret_cast<ObjectBase*>(reinterpret_cast<UIntPtr>(header) + sizeof(ObjectHeader));

    return ptr;
}

void ObjectHeader::DestructThisObject(ObjectHeader* header)
{
    AssertDebug(header != nullptr);
    AssertDebug(header->cls != nullptr);

    // get pointer to object
    ObjectBase* ptr = reinterpret_cast<ObjectBase*>(reinterpret_cast<UIntPtr>(header) + sizeof(ObjectHeader));

    ptr->~ObjectBase();
}

#pragma endregion ObjectHeader

#pragma region ObjectBase

ObjectBase::ObjectBase()
    : m_initState(INIT_STATE_UNINITIALIZED)
{
#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    m_scriptObjectResource = nullptr;
#endif

#ifndef HYP_TOOL // If we're building the Build Tool we won't have access to Class data
    // get the header by subtracting the offset from this pointer
    m_header = reinterpret_cast<ObjectHeader*>(UIntPtr(this) - sizeof(ObjectHeader));
#endif
}

ObjectBase::~ObjectBase()
{
#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)

    if (m_scriptObjectResource)
    {
#ifdef HYP_SCRIPT
        // destruct all dynamic fields
        if ((m_scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::HypScript))) && m_header->cls->IsDynamic())
        {
            const Class* cls = m_header->cls;

            SizeType fieldOffset = sizeof(ObjectBase);

            // `class` field
            fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(ClassRef));
            ClassRef* classFieldPtr = (ClassRef*)(UIntPtr(this) + fieldOffset);
            fieldOffset += sizeof(ClassRef);

            while (cls != nullptr && cls->IsDynamic())
            {
                for (Field* field : cls->GetFields())
                {
                    // align field offset
                    fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(BoxedValue));

                    BoxedValue* fieldPtr = (BoxedValue*)(UIntPtr(this) + fieldOffset);
                    fieldPtr->~BoxedValue();

                    fieldOffset += sizeof(BoxedValue);
                }

                cls = cls->GetParent();
            }

            // destruct the `class` field last:
            classFieldPtr->~ClassRef();
        }
#endif

        delete m_scriptObjectResource;
        m_scriptObjectResource = nullptr;
    }
#endif
}

ObjIdBase ObjectBase::Id() const
{
    HYP_CORE_ASSERT(m_header, "Invalid Object!");

    return ObjIdBase { m_header->cls->GetTypeId(), m_header->index + 1 };
}

const Class* ObjectBase::StaticClass()
{
    return g_clsObjectBase;
}

const Class* ObjectBase::InstanceClass() const
{
    HYP_CORE_ASSERT(m_header, "Invalid Object!");
    HYP_CORE_ASSERT(m_header->cls, "No Class defined for type");

    return m_header->cls;
}

int32 ObjectBase::AddRef()
{
    return m_header->IncRefStrong();
}

int32 ObjectBase::Release()
{
    uint32 count = m_header->DecRefStrong();
    AssertDebug(count >= 0);

    return count;
}

#ifdef HYP_DOTNET
dotnet::ManagedObject* ObjectBase::GetManagedObject() const
{
    return m_scriptObjectResource ? m_scriptObjectResource->GetManagedObject() : nullptr;
}
#endif

Pool* ObjectBase::GetAllocator()
{
    return g_objectPool;
}

#pragma endregion ObjectBase

#pragma region TypedObjPtr

HYP_API uint32 TypedObjPtr::GetRefCountStrong() const
{
    if (!IsValid())
    {
        return 0;
    }

    ObjectBase* casted = reinterpret_cast<ObjectBase*>(m_ptr);

    return casted->GetObjectHeader_Internal()->GetRefCountStrong();
}

HYP_API uint32 TypedObjPtr::GetRefCountWeak() const
{
    if (!IsValid())
    {
        return 0;
    }

    ObjectBase* casted = reinterpret_cast<ObjectBase*>(m_ptr);

    return casted->GetObjectHeader_Internal()->GetRefCountWeak();
}

HYP_API void TypedObjPtr::IncRef(bool weak)
{
    AssertDebug(IsValid());

    ObjectBase* casted = reinterpret_cast<ObjectBase*>(m_ptr);

    if (weak)
    {
        casted->GetObjectHeader_Internal()->IncRefWeak();
    }
    else
    {
        casted->GetObjectHeader_Internal()->IncRefStrong();
    }
}

HYP_API void TypedObjPtr::DecRef(bool weak)
{
    AssertDebug(IsValid());

    ObjectBase* casted = reinterpret_cast<ObjectBase*>(m_ptr);

    if (weak)
    {
        casted->GetObjectHeader_Internal()->DecRefWeak();
    }
    else
    {
        casted->GetObjectHeader_Internal()->DecRefStrong();
    }
}

#pragma endregion TypedObjPtr

} // namespace Hyperion
