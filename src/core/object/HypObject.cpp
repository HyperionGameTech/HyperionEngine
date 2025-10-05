/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypObject.hpp>
#include <core/object/HypObjectPool.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/containers/Stack.hpp>

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)

#ifdef HYP_DOTNET
#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#endif

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#endif

#include <scripting/ScriptObjectResource.hpp>

#endif

namespace hyperion {

#pragma region HypObjectInitializerGuardBase

HypObjectInitializerGuardBase::HypObjectInitializerGuardBase(HypObjectPtr ptr)
    : ptr(ptr)
{
    HYP_CORE_ASSERT(ptr.IsValid());

    count = 0;

    HYP_CORE_ASSERT(ptr.GetClass()->UseHandles());

    HypObjectBase* target = reinterpret_cast<HypObjectBase*>(ptr.GetPointer());
    HYP_CORE_ASSERT(target != nullptr, "HypObjectInitializerGuardBase: HypObjectPtr is not valid!");

    // Push NONE to prevent our current flags from polluting allocations that happen in the constructor
    PushGlobalContext(HypObjectInitializerContext {
        .hypClass = ptr.GetClass(),
        .flags = HypObjectInitializerFlags::NONE });
}

HypObjectInitializerGuardBase::~HypObjectInitializerGuardBase()
{
    PopGlobalContext<HypObjectInitializerContext>();

    if (!ptr.IsValid())
    {
        return;
    }

    const HypClass* hypClass = ptr.GetClass();
    HYP_CORE_ASSERT(hypClass->UseHandles()); // check is HypObjectBase

    HypObjectBase* target = reinterpret_cast<HypObjectBase*>(ptr.GetPointer());
    AssertDebug(target->GetObjectHeader_Internal()->GetRefCountStrong() == 1);

    if (!(ptr.GetClass()->GetFlags() & HypClassFlags::NO_SCRIPT_BINDINGS))
    {
        HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();

        if ((!context || !(context->flags & HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)) && !hypClass->IsAbstract())
        {
#ifdef HYP_DOTNET
            if (RC<dotnet::Class> managedClass = hypClass->GetManagedClass())
            {
                ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>(ptr, managedClass);

                Assert(scriptObjectResource != nullptr);
                scriptObjectResource->IncRef();

                target->SetScriptObjectResource(scriptObjectResource);

                return;
            }
#endif

#ifdef HYP_SCRIPT
            HypData obj;

            ScriptObjectResource* scriptObjectResource = AllocateResource<ScriptObjectResource>((Script_Instance*)nullptr, std::move(obj));
            Assert(scriptObjectResource != nullptr);

            target->SetScriptObjectResource(scriptObjectResource);

            return;
#endif
        }
    }
}

#pragma endregion HypObjectInitializerGuardBase

#pragma region HypObjectHeader

HypObjectBase* HypObjectHeader::GetObjectPointer(HypObjectHeader* header)
{
    AssertDebug(header != nullptr);
    AssertDebug(header->hypClass != nullptr);

    // calculate offset of the object data after the header
    const SizeType alignment = header->hypClass->GetAlignment();
    const SizeType objectOffset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    // get pointer to object
    HypObjectBase* ptr = reinterpret_cast<HypObjectBase*>(reinterpret_cast<UIntPtr>(header) + objectOffset);

    return ptr;
}

void HypObjectHeader::DestructThisObject(HypObjectHeader* header)
{
    AssertDebug(header != nullptr);
    AssertDebug(header->hypClass != nullptr);

    // calculate offset of the object data after the header
    const SizeType alignment = header->hypClass->GetAlignment();
    const SizeType objectOffset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    // get pointer to object
    HypObjectBase* ptr = reinterpret_cast<HypObjectBase*>(reinterpret_cast<UIntPtr>(header) + objectOffset);

    ptr->~HypObjectBase();
}

#pragma endregion HypObjectHeader

#pragma region HypObjectBase

HypObjectBase::HypObjectBase()
    : m_initState(INIT_STATE_UNINITIALIZED)
{
#ifndef HYP_BUILDTOOL // If we're building the Build Tool we won't have access to HypClass data
    HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();
    HYP_CORE_ASSERT(context != nullptr);

    const SizeType alignment = context->hypClass->GetAlignment();
    HYP_CORE_ASSERT(alignment != 0);

    // calculate offset of the object data after the header
    const SizeType objectOffset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    // get the header by subtracting the offset from this pointer
    m_header = reinterpret_cast<HypObjectHeader*>(UIntPtr(this) - objectOffset);

    // increment the strong reference count for the Handle<T> that will be returned from CreateObject<T>().
    AtomicIncrement(&m_header->refCountStrong);
    HYP_CORE_ASSERT(m_header->refCountStrong == 1);
#endif
}

HypObjectBase::~HypObjectBase()
{
    m_delegateHandlers = {};

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)

    if (m_scriptObjectResource)
    {
#ifdef HYP_SCRIPT
        // destruct all dynamic fields
        if (m_scriptObjectResource->GetScriptLanguage() == SL_HYPSCRIPT && m_header->hypClass->IsDynamic())
        {
            const HypClass* hypClass = m_header->hypClass;

            SizeType fieldOffset = sizeof(HypObjectBase);

            // `class` field
            fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypClassRef));
            HypClassRef* classFieldPtr = (HypClassRef*)(UIntPtr(this) + fieldOffset);
            fieldOffset += sizeof(HypClassRef);

            while (hypClass != nullptr && hypClass->IsDynamic())
            {
                for (HypField* field : hypClass->GetFields())
                {
                    // align field offset
                    fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypData));

                    HypData* fieldPtr = (HypData*)(UIntPtr(this) + fieldOffset);
                    fieldPtr->~HypData();

                    fieldOffset += sizeof(HypData);
                }

                hypClass = hypClass->GetParent();
            }

            // destruct the `class` field last:
            classFieldPtr->~HypClassRef();
        }
#endif

        FreeResource(m_scriptObjectResource);
        m_scriptObjectResource = nullptr;
    }
#endif
}

#ifdef HYP_DOTNET
dotnet::Object* HypObjectBase::GetManagedObject() const
{
    return m_scriptObjectResource ? m_scriptObjectResource->GetManagedObject() : nullptr;
}
#endif

#pragma endregion HypObjectBase

#pragma region HypObjectPtr

HYP_API uint32 HypObjectPtr::GetRefCountStrong() const
{
    if (!IsValid())
    {
        return 0;
    }

    HypObjectBase* hypObjectBase = reinterpret_cast<HypObjectBase*>(m_ptr);

    return hypObjectBase->GetObjectHeader_Internal()->GetRefCountStrong();
}

HYP_API uint32 HypObjectPtr::GetRefCountWeak() const
{
    if (!IsValid())
    {
        return 0;
    }

    HypObjectBase* hypObjectBase = reinterpret_cast<HypObjectBase*>(m_ptr);

    return hypObjectBase->GetObjectHeader_Internal()->GetRefCountWeak();
}

HYP_API void HypObjectPtr::IncRef(bool weak)
{
    HYP_CORE_ASSERT(IsValid());

    HypObjectBase* hypObjectBase = reinterpret_cast<HypObjectBase*>(m_ptr);

    if (weak)
    {
        hypObjectBase->GetObjectHeader_Internal()->IncRefWeak();
    }
    else
    {
        hypObjectBase->GetObjectHeader_Internal()->IncRefStrong();
    }
}

HYP_API void HypObjectPtr::DecRef(bool weak)
{
    HYP_CORE_ASSERT(IsValid());

    HypObjectBase* hypObjectBase = reinterpret_cast<HypObjectBase*>(m_ptr);

    if (weak)
    {
        hypObjectBase->GetObjectHeader_Internal()->DecRefWeak();
    }
    else
    {
        hypObjectBase->GetObjectHeader_Internal()->DecRefStrong();
    }
}

#pragma endregion HypObjectPtr

} // namespace hyperion
