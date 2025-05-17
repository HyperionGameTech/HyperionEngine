/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypObject.hpp>
#include <core/ObjectPool.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/containers/Stack.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

namespace hyperion {

struct HypObjectInitializerContext
{
    const HypClass  *hyp_class = nullptr;
    const void      *address = nullptr;
};

thread_local Stack<EnumFlags<HypObjectInitializerFlags>> g_hyp_object_initializer_flags_stack = { };

void InitHypObjectInitializer(HypObjectPtr ptr);

HYP_API void PushHypObjectInitializerFlags(EnumFlags<HypObjectInitializerFlags> flags)
{
    g_hyp_object_initializer_flags_stack.Push(flags);
}

HYP_API void PopHypObjectInitializerFlags()
{
    AssertThrowMsg(g_hyp_object_initializer_flags_stack.Any(), "No HypObjectInitializerFlags to pop! Push/pop out of sync somewhere?");

    g_hyp_object_initializer_flags_stack.Pop();
}

static EnumFlags<HypObjectInitializerFlags> GetCurrentHypObjectInitializerFlags()
{
    if (!g_hyp_object_initializer_flags_stack.Any()) {
        return HypObjectInitializerFlags::NONE;
    }

    return g_hyp_object_initializer_flags_stack.Top();
}

#pragma region HypObjectInitializerGuardBase

HypObjectInitializerGuardBase::HypObjectInitializerGuardBase(HypObjectPtr ptr)
    : ptr(ptr)
{
#ifdef HYP_DEBUG_MODE
    initializer_thread_id = Threads::CurrentThreadID();
#else
    count = 0;
#endif

    // Push NONE to prevent our current flags from polluting allocations that happen in the constructor
    PushHypObjectInitializerFlags(HypObjectInitializerFlags::NONE);
}

HypObjectInitializerGuardBase::~HypObjectInitializerGuardBase()
{
    PopHypObjectInitializerFlags();

    if (!ptr.IsValid()) {
        return;
    }

    IHypObjectInitializer *initializer = ptr.GetObjectInitializer();
    AssertThrow(initializer != nullptr);

    ManagedObjectResource *managed_object_resource = nullptr;

    if (!(GetCurrentHypObjectInitializerFlags() & HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION) && !ptr.GetClass()->IsAbstract()) {
        if (dotnet::Class *managed_class = ptr.GetClass()->GetManagedClass()) {
            managed_object_resource = AllocateResource<ManagedObjectResource>(ptr);
            initializer->SetManagedObjectResource(managed_object_resource);
        }
    }

    InitHypObjectInitializer(ptr);
}

#pragma endregion HypObjectInitializerGuardBase

#pragma region HypObjectBase

TypeID HypObjectBase::GetTypeID() const
{
    return m_header->container->GetObjectTypeID();
}

const HypClass *HypObjectBase::InstanceClass() const
{
    return m_header->container->GetObjectClass();
}

IDBase HypObjectBase::GetID_Internal() const
{
    return IDBase { m_header->index + 1 };
}

#pragma endregion HypObjectBase

#pragma region HypObjectPtr

HYP_API uint32 HypObjectPtr::GetRefCount_Strong() const
{
    if (!IsValid()) {
        return 0;
    }

    IHypObjectInitializer *initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    return initializer->GetRefCount_Strong(m_hyp_class->GetAllocationMethod(), m_ptr);
}

HYP_API uint32 HypObjectPtr::GetRefCount_Weak() const
{
    if (!IsValid()) {
        return 0;
    }

    IHypObjectInitializer *initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    return initializer->GetRefCount_Weak(m_hyp_class->GetAllocationMethod(), m_ptr);
}

HYP_DISABLE_OPTIMIZATION;
HYP_API void HypObjectPtr::IncRef(bool weak)
{
    AssertDebug(IsValid());

    IHypObjectInitializer *initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    initializer->IncRef(m_hyp_class->GetAllocationMethod(), m_ptr, weak);
}

HYP_API void HypObjectPtr::DecRef(bool weak)
{
    AssertDebug(IsValid());

    IHypObjectInitializer *initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    initializer->DecRef(m_hyp_class->GetAllocationMethod(), m_ptr, weak);
}
HYP_ENABLE_OPTIMIZATION;

HYP_API IHypObjectInitializer *HypObjectPtr::GetObjectInitializer() const
{
    if (!m_hyp_class || !m_ptr) {
        return nullptr;
    }

    return m_hyp_class->GetObjectInitializer(m_ptr);
}

HYP_API const HypClass *HypObjectPtr::GetHypClass(TypeID type_id) const
{
    const HypClass *hyp_class = ::hyperion::GetClass(type_id);
    AssertThrow(hyp_class != nullptr);

    if (m_ptr != nullptr) {
        const IHypObjectInitializer *initializer = hyp_class->GetObjectInitializer(m_ptr);
        AssertThrow(initializer != nullptr);

        hyp_class = initializer->GetClass();
        AssertThrow(hyp_class != nullptr);
    }

    return hyp_class;
}

#pragma endregion HypObjectPtr

HYP_API void CheckHypObjectInitializer(const IHypObjectInitializer *initializer, TypeID type_id, const HypClass *hyp_class, const void *address)
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(initializer != nullptr);
    AssertThrow(hyp_class != nullptr);
    AssertThrow(address != nullptr);
#endif

    // If allocation method is NONE we don't require the guard; skip this check.
    if (hyp_class->GetAllocationMethod() == HypClassAllocationMethod::NONE) {
        return;
    }
}

void InitHypObjectInitializer(HypObjectPtr ptr)
{
    AssertThrow(ptr.IsValid());

    if (ptr.GetClass()->GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR) {
        // Hack to make EnableRefCountedPtr<Base> internally have TypeID of most derived class for a given instance.
        static_cast<EnableRefCountedPtrFromThisBase<> *>(ptr.GetPointer())->weak_this.GetRefCountData_Internal()->type_id = ptr.GetClass()->GetTypeID();
    }

    // Fixup the pointers on objects up the chain - all should point to the HypObjectInitializer for the most derived class.
    IHypObjectInitializer *parent_initializer = ptr.GetObjectInitializer();
    AssertThrow(parent_initializer != nullptr);

    do {
        if (const HypClass *parent_hyp_class = parent_initializer->GetClass()->GetParent()) {
            parent_initializer = parent_hyp_class->GetObjectInitializer(ptr.GetPointer());
            AssertThrow(parent_initializer != nullptr);

            parent_initializer->FixupPointer(ptr.GetPointer(), ptr.GetObjectInitializer());
        } else {
            parent_initializer = nullptr;
        }
    } while (parent_initializer != nullptr);
}

HYP_API HypClassAllocationMethod GetHypClassAllocationMethod(const HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);
    
    return hyp_class->GetAllocationMethod();
}

HYP_API dotnet::Class *GetHypClassManagedClass(const HypClass *hyp_class)
{
    AssertThrow(hyp_class != nullptr);

    return hyp_class->GetManagedClass();
}

HYP_DISABLE_OPTIMIZATION;

HYP_API void HypObject_OnIncRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (IHypObjectInitializer *initializer = ptr.GetObjectInitializer()) {
        if (count > 1) {
            if (ManagedObjectResource *managed_object_resource = initializer->GetManagedObjectResource()) {
                managed_object_resource->Claim();
            }
        }
    }
}

HYP_API void HypObject_OnDecRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (const IHypObjectInitializer *initializer = ptr.GetObjectInitializer()) {
        if (count >= 1) {
            if (ManagedObjectResource *managed_object_resource = initializer->GetManagedObjectResource()) {
                managed_object_resource->Unclaim();
            }
        }
    }
}
HYP_ENABLE_OPTIMIZATION;

HYP_API void HypObject_OnIncRefCount_Weak(HypObjectPtr ptr, uint32 count)
{
}

HYP_API void HypObject_OnDecRefCount_Weak(HypObjectPtr ptr, uint32 count)
{
}

} // namespace hyperion