/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/HypObject.hpp>
#include <core/reflection/HypObjectPool.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/containers/Stack.hpp>

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

namespace hyperion {

#ifdef HYP_BUILDTOOL
const Class* g_clsHypObjectBase = nullptr;
#else
HYP_API extern const Class* g_clsHypObjectBase;
#endif

#pragma region HypObjectInitializerGuardBase

HypObjectInitializerGuardBase::HypObjectInitializerGuardBase(HypObjectPtr ptr)
    : ptr(ptr)
{
    AssertDebug(ptr.IsValid());

    count = 0;

    AssertDebug(ptr.GetClass()->UseHandles());

    HypObjectBase* target = reinterpret_cast<HypObjectBase*>(ptr.GetPointer());
    AssertDebug(target != nullptr, "HypObjectInitializerGuardBase: HypObjectPtr is not valid!");

    // Push NONE to prevent our current flags from polluting allocations that happen in the constructor
    PushGlobalContext(HypObjectInitializerContext {
        .cls = ptr.GetClass(),
        .flags = HypObjectInitializerFlags::NONE });
}

HypObjectInitializerGuardBase::~HypObjectInitializerGuardBase()
{
    PopGlobalContext<HypObjectInitializerContext>();

    if (!ptr.IsValid())
    {
        return;
    }

    const Class* cls = ptr.GetClass();
    AssertDebug(cls->UseHandles()); // check is HypObjectBase

    HypObjectBase* target = reinterpret_cast<HypObjectBase*>(ptr.GetPointer());
    AssertDebug(target->GetObjectHeader_Internal()->GetRefCountStrong() == 1);

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    AssertDebug(target->GetScriptObjectResource() == nullptr);
#endif

    if (!(ptr.GetClass()->GetFlags() & ClassFlags::NO_SCRIPT_BINDINGS))
    {
        HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();

        if ((!context || !(context->flags & HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)) && !cls->IsAbstract())
        {
#ifdef HYP_DOTNET
            if (RC<dotnet::ManagedClass> managedClass = cls->GetManagedClass())
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
    AssertDebug(header->cls != nullptr);

    // get pointer to object
    HypObjectBase* ptr = reinterpret_cast<HypObjectBase*>(reinterpret_cast<UIntPtr>(header) + sizeof(HypObjectHeader));

    return ptr;
}

void HypObjectHeader::DestructThisObject(HypObjectHeader* header)
{
    AssertDebug(header != nullptr);
    AssertDebug(header->cls != nullptr);

    // get pointer to object
    HypObjectBase* ptr = reinterpret_cast<HypObjectBase*>(reinterpret_cast<UIntPtr>(header) + sizeof(HypObjectHeader));

    ptr->~HypObjectBase();
}

#pragma endregion HypObjectHeader

#pragma region HypObjectBase

HypObjectBase::HypObjectBase()
    : m_initState(INIT_STATE_UNINITIALIZED)
{
#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    m_scriptObjectResource = nullptr;
#endif

#ifndef HYP_BUILDTOOL // If we're building the Build Tool we won't have access to Class data
    // get the header by subtracting the offset from this pointer
    m_header = reinterpret_cast<HypObjectHeader*>(UIntPtr(this) - sizeof(HypObjectHeader));
#endif
}

HypObjectBase::~HypObjectBase()
{
#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)

    if (m_scriptObjectResource)
    {
#ifdef HYP_SCRIPT
        // destruct all dynamic fields
        if (m_scriptObjectResource->GetScriptLanguage() == SL_HYPSCRIPT && m_header->cls->IsDynamic())
        {
            const Class* cls = m_header->cls;

            SizeType fieldOffset = sizeof(HypObjectBase);

            // `class` field
            fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(ClassRef));
            ClassRef* classFieldPtr = (ClassRef*)(UIntPtr(this) + fieldOffset);
            fieldOffset += sizeof(ClassRef);

            while (cls != nullptr && cls->IsDynamic())
            {
                for (Field* field : cls->GetFields())
                {
                    // align field offset
                    fieldOffset = ByteUtil::AlignAs(fieldOffset, alignof(HypData));

                    HypData* fieldPtr = (HypData*)(UIntPtr(this) + fieldOffset);
                    fieldPtr->~HypData();

                    fieldOffset += sizeof(HypData);
                }

                cls = cls->GetParent();
            }

            // destruct the `class` field last:
            classFieldPtr->~ClassRef();
        }
#endif

        FreeResource(m_scriptObjectResource);
        m_scriptObjectResource = nullptr;
    }
#endif
}

const Class* HypObjectBase::StaticClass()
{
    return g_clsHypObjectBase;
}

#ifdef HYP_DOTNET
dotnet::ManagedObject* HypObjectBase::GetManagedObject() const
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
    AssertDebug(IsValid());

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
    AssertDebug(IsValid());

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
