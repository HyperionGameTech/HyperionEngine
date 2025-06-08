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

#pragma region HypObjectInitializerGuardBase

HypObjectInitializerGuardBase::HypObjectInitializerGuardBase(HypObjectPtr ptr)
    : ptr(ptr)
{
    AssertThrow(ptr.IsValid());

#ifdef HYP_DEBUG_MODE
    initializer_thread_id = Threads::CurrentThreadID();
#else
    count = 0;
#endif

    // Push NONE to prevent our current flags from polluting allocations that happen in the constructor
    PushGlobalContext(HypObjectInitializerContext {
        .hyp_class = ptr.GetClass(),
        .flags = HypObjectInitializerFlags::NONE });
}

HypObjectInitializerGuardBase::~HypObjectInitializerGuardBase()
{
    PopGlobalContext<HypObjectInitializerContext>();

    if (!ptr.IsValid())
    {
        return;
    }

    HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();

    if ((!context || !(context->flags & HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)) && !ptr.GetClass()->IsAbstract())
    {
        if (ptr.GetClass()->GetManagedClass() != nullptr)
        {
            ptr.GetObjectInitializer()->SetManagedObjectResource(AllocateResource<ManagedObjectResource>(ptr));
        }
        else
        {
            HYP_LOG(Object, Warning,
                "HypObjectInitializerGuardBase: HypClass '{}' does not have a managed class associated with it. "
                "This means that the object will not be created in the managed runtime, and will not be accessible from C#.",
                ptr.GetClass()->GetName());
        }
    }

    FixupObjectInitializerPointer(ptr.GetPointer(), ptr.GetObjectInitializer());
}

#pragma endregion HypObjectInitializerGuardBase

#pragma region HypObjectBase

HypObjectBase::HypObjectBase()
{
    HypObjectInitializerContext* context = GetGlobalContext<HypObjectInitializerContext>();
    AssertThrow(context != nullptr);

    const SizeType size = context->hyp_class->GetSize();
    AssertDebug(size > 0);

    const SizeType alignment = context->hyp_class->GetAlignment();
    AssertDebug(alignment > 0);

    const SizeType header_offset = ((sizeof(HypObjectHeader) + alignment - 1) / alignment) * alignment;

    m_header = reinterpret_cast<HypObjectHeader*>(uintptr_t(this) - header_offset);
}

HypObjectBase::~HypObjectBase()
{
    AssertDebug(m_header != nullptr);
    m_header->DecRefWeak();
}

IDBase HypObjectBase::GetID_Internal() const
{
    return IDBase { m_header->index + 1 };
}

#pragma endregion HypObjectBase

#pragma region HypObjectPtr

HYP_API uint32 HypObjectPtr::GetRefCount_Strong() const
{
    if (!IsValid())
    {
        return 0;
    }

    IHypObjectInitializer* initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    return initializer->GetRefCount_Strong(m_ptr);
}

HYP_API uint32 HypObjectPtr::GetRefCount_Weak() const
{
    if (!IsValid())
    {
        return 0;
    }

    IHypObjectInitializer* initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    return initializer->GetRefCount_Weak(m_ptr);
}

HYP_API void HypObjectPtr::IncRef(bool weak)
{
    AssertDebug(IsValid());

    IHypObjectInitializer* initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    initializer->IncRef(m_ptr, weak);
}

HYP_API void HypObjectPtr::DecRef(bool weak)
{
    AssertDebug(IsValid());

    IHypObjectInitializer* initializer = m_hyp_class->GetObjectInitializer(m_ptr);
    AssertDebug(initializer != nullptr);

    initializer->DecRef(m_ptr, weak);
}

HYP_API IHypObjectInitializer* HypObjectPtr::GetObjectInitializer() const
{
    if (!m_hyp_class || !m_ptr)
    {
        return nullptr;
    }

    return m_hyp_class->GetObjectInitializer(m_ptr);
}

HYP_API const HypClass* HypObjectPtr::GetHypClass(TypeID type_id) const
{
    const HypClass* hyp_class = ::hyperion::GetClass(type_id);
    AssertThrow(hyp_class != nullptr);

    if (m_ptr != nullptr)
    {
        const IHypObjectInitializer* initializer = hyp_class->GetObjectInitializer(m_ptr);
        AssertThrow(initializer != nullptr);

        hyp_class = initializer->GetClass();
        AssertThrow(hyp_class != nullptr);
    }

    return hyp_class;
}

#pragma endregion HypObjectPtr

#pragma region DynamicHypObjectInitializer

DynamicHypObjectInitializer::DynamicHypObjectInitializer(const HypClass* hyp_class, IHypObjectInitializer* parent_initializer)
    : m_hyp_class(hyp_class),
      m_parent_initializer(parent_initializer)
{
    AssertThrow(hyp_class != nullptr);
    AssertThrow(hyp_class->IsDynamic());

    AssertThrowMsg(hyp_class->IsReferenceCounted(),
        "DynamicHypObjectInitializer: HypClass is not reference counted. Class: %s"
        "\n\tDynamic HypClass types must be reference counted to ensure proper cleanup and deletion of allocations.",
        *hyp_class->GetName());

    AssertThrow(parent_initializer != nullptr);

    AssertThrowMsg(parent_initializer->GetClass() == hyp_class->GetParent(),
        "DynamicHypObjectInitializer: Parent initializer class does not match hyp_class parent class. Parent initializer class: %s, hyp_class parent class: %s",
        *parent_initializer->GetClass()->GetName(), *hyp_class->GetParent()->GetName());
}

DynamicHypObjectInitializer::~DynamicHypObjectInitializer()
{
    // delete up the chain, if the parent is also dynamic.
    if (m_parent_initializer->GetClass()->IsDynamic())
    {
        delete m_parent_initializer;
    }
}

TypeID DynamicHypObjectInitializer::GetTypeID() const
{
    return m_hyp_class->GetTypeID();
}

#pragma endregion DynamicHypObjectInitializer

HYP_API void FixupObjectInitializerPointer(void* target, IHypObjectInitializer* initializer)
{
    const HypClass* hyp_class = initializer->GetClass();

    while (hyp_class != nullptr)
    {
        hyp_class->FixupPointer(target, initializer);

        const HypClass* parent_hyp_class = hyp_class->GetParent();

        // Check to ensure no non-dynamic initializers are in the chain after the first dynamic one found.
        // This ensures we can properly delete the dynamic initializers up the chain, when the reference count goes to 0.
        if (parent_hyp_class && parent_hyp_class->IsDynamic())
        {
            AssertThrowMsg(hyp_class->IsDynamic(),
                "Found a dynamic initializer in the chain, but the current initializer is not dynamic. Only dynamic initializers are allowed proceeding a dynamic initializer.\n\tCurrent initializer class: %s, next initializer class: %s",
                *hyp_class->GetName(), *parent_hyp_class->GetName());
        }

        hyp_class = parent_hyp_class;
    }
}

HYP_API void HypObject_OnIncRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (IHypObjectInitializer* initializer = ptr.GetObjectInitializer())
    {
        if (count > 1)
        {
            if (ManagedObjectResource* managed_object_resource = initializer->GetManagedObjectResource())
            {
                managed_object_resource->IncRef();
            }
        }
    }
}

HYP_API void HypObject_OnDecRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (IHypObjectInitializer* initializer = ptr.GetObjectInitializer())
    {
        if (count >= 1)
        {
            if (ManagedObjectResource* managed_object_resource = initializer->GetManagedObjectResource())
            {
                managed_object_resource->DecRef();
            }
        }
        else if (initializer->GetClass()->IsDynamic())
        {
            // Dynamic HypClass initializers need to be deleted manually, as they are not stored inline on the class and are heap allocated.
            // They will delete up the chain if the parent is also dynamic.
            // We don't allow any non-dynamic initializers in the chain after a dynamic one is added.
            delete initializer;
        }
    }
}

HYP_API void HypObject_OnIncRefCount_Weak(HypObjectPtr ptr, uint32 count)
{
}

HYP_API void HypObject_OnDecRefCount_Weak(HypObjectPtr ptr, uint32 count)
{
}

} // namespace hyperion