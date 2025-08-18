/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypObject.hpp>
#include <core/object/HypObjectPool.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/containers/Stack.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

namespace hyperion {

#pragma region HypObjectInitializerGuardBase

HypObjectInitializerGuardBase::HypObjectInitializerGuardBase(HypObjectPtr ptr)
    : ptr(ptr)
{
    HYP_CORE_ASSERT(ptr.IsValid());

#ifdef HYP_DEBUG_MODE
    initializerThreadId = Threads::CurrentThreadId();
#else
    count = 0;
#endif

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
            if (RC<dotnet::Class> managedClass = hypClass->GetManagedClass())
            {
                ManagedObjectResource* managedObjectResource = AllocateResource<ManagedObjectResource>(ptr, managedClass);

                Assert(managedObjectResource != nullptr);
                managedObjectResource->IncRef();

                target->SetManagedObjectResource(managedObjectResource);
            }
            else
            {
                HYP_LOG(Object, Warning,
                    "HypObjectInitializerGuardBase: HypClass '{}' does not have a managed class associated with it. "
                    "This means that the object will not be created in the managed runtime, and will not be accessible from C#.",
                    hypClass->GetName());
            }
        }
    }
}

#pragma endregion HypObjectInitializerGuardBase

#pragma region HypObjectHeader

HypObjectBase* HypObjectHeader::GetObjectPointer(HypObjectHeader* header)
{
    AssertDebug(header != nullptr);
    AssertDebug(header->hypClass != nullptr);

    // get offset
    const SizeType alignment = header->hypClass->GetAlignment();
    const SizeType headerOffset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    // get pointer to object
    HypObjectBase* ptr = reinterpret_cast<HypObjectBase*>(reinterpret_cast<uintptr_t>(header) + headerOffset);

    return ptr;
}

void HypObjectHeader::DestructThisObject(HypObjectHeader* header)
{
    AssertDebug(header != nullptr);
    AssertDebug(header->hypClass != nullptr);

    // get offset
    const SizeType alignment = header->hypClass->GetAlignment();
    const SizeType headerOffset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    // get pointer to object
    HypObjectBase* ptr = reinterpret_cast<HypObjectBase*>(reinterpret_cast<uintptr_t>(header) + headerOffset);

    ptr->~HypObjectBase();
}

#pragma endregion HypObjectHeader

#pragma region HypObjectBase

HypObjectBase::HypObjectBase()
    : m_initState(INIT_STATE_UNINITIALIZED)
{
    HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();
    HYP_CORE_ASSERT(context != nullptr);

    const SizeType size = context->hypClass->GetSize();
    const SizeType alignment = context->hypClass->GetAlignment();

    HYP_CORE_ASSERT(size != 0 && alignment != 0);

    const SizeType headerOffset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    m_header = reinterpret_cast<HypObjectHeader*>(uintptr_t(this) - headerOffset);

    // increment the strong reference count for the Handle<T> that will be returned from CreateObject<T>().
    AtomicIncrement(&m_header->refCountStrong);
}

HypObjectBase::~HypObjectBase()
{
    HYP_CORE_ASSERT(m_header != nullptr);

    if (m_managedObjectResource)
    {
        FreeResource(m_managedObjectResource);
        m_managedObjectResource = nullptr;
    }
}

#pragma endregion HypObjectBase

#pragma region HypObjectPtr

HYP_API uint32 HypObjectPtr::GetRefCountStrong() const
{
    if (!IsValid())
    {
        return 0;
    }

    HYP_CORE_ASSERT(m_hypClass->UseHandles()); // check is HypObjectBase

    HypObjectBase* hypObjectBase = reinterpret_cast<HypObjectBase*>(m_ptr);

    return hypObjectBase->GetObjectHeader_Internal()->GetRefCountStrong();
}

HYP_API uint32 HypObjectPtr::GetRefCountWeak() const
{
    if (!IsValid())
    {
        return 0;
    }

    HYP_CORE_ASSERT(m_hypClass->UseHandles()); // check is HypObjectBase

    HypObjectBase* hypObjectBase = reinterpret_cast<HypObjectBase*>(m_ptr);

    return hypObjectBase->GetObjectHeader_Internal()->GetRefCountWeak();
}

HYP_API void HypObjectPtr::IncRef(bool weak)
{
    HYP_CORE_ASSERT(IsValid());

    HYP_CORE_ASSERT(m_hypClass->UseHandles()); // check is HypObjectBase

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

    HYP_CORE_ASSERT(m_hypClass->UseHandles()); // check is HypObjectBase

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

HYP_API void HypObject_AcquireManagedObjectLock(HypObjectBase* ptr)
{
    AssertDebug(ptr->GetObjectHeader_Internal()->GetRefCountStrong() > 1);
    if (ManagedObjectResource* managedObjectResource = ptr->GetManagedObjectResource())
    {
        managedObjectResource->IncRef();
    }
}

HYP_API void HypObject_ReleaseManagedObjectLock(HypObjectBase* ptr)
{
    if (ManagedObjectResource* managedObjectResource = ptr->GetManagedObjectResource())
    {
        managedObjectResource->DecRef();
    }
}

} // namespace hyperion
