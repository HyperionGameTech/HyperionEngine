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

thread_local Stack<HypObjectInitializerContext> g_contexts = { };
thread_local Stack<EnumFlags<HypObjectInitializerFlags>> g_hyp_object_initializer_flags_stack = { };

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

HypObjectInitializerGuardBase::HypObjectInitializerGuardBase(const HypClass *hyp_class, void *address)
    : hyp_class(hyp_class),
      address(address)
{
#ifdef HYP_DEBUG_MODE
    initializer_thread_id = Threads::CurrentThreadID();
#else
    count = 0;
#endif

    if (hyp_class != nullptr) {
        Stack<const HypClass *> hyp_classes;

        const HypClass *current = hyp_class;

        while (current != nullptr) {
            hyp_classes.Push(current);

            current = current->GetParent();
        }

        while (hyp_classes.Any()) {
            g_contexts.Push({ hyp_classes.Pop(), address });

#ifndef HYP_DEBUG_MODE
            count++;
#endif
        }
    }
}

HypObjectInitializerGuardBase::~HypObjectInitializerGuardBase()
{
    if (!hyp_class) {
        return;
    }

    IHypObjectInitializer *initializer = hyp_class->GetObjectInitializer(address);
    AssertThrow(initializer != nullptr);

    dotnet::Object *managed_object = nullptr;

    if (!(GetCurrentHypObjectInitializerFlags() & HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION) && !hyp_class->IsAbstract()) {
        if (dotnet::Class *managed_class = hyp_class->GetManagedClass()) {
            managed_object = managed_class->NewObject(hyp_class, address);
        }
    }

    InitHypObjectInitializer(initializer, address, hyp_class->GetTypeID(), hyp_class, managed_object);

#ifdef HYP_DEBUG_MODE
    AssertThrow(initializer_thread_id == Threads::CurrentThreadID());

    Queue<const HypClass *> hyp_classes;

    const HypClass *current = hyp_class;

    while (current != nullptr) {
        hyp_classes.Push(current);

        current = current->GetParent();
    }

    while (hyp_classes.Any()) {
        AssertThrow(g_contexts.Pop().hyp_class == hyp_classes.Pop());
    }
#else
    for (uint32 i = 0; i < count; i++) {
        g_contexts.Pop();
    }
#endif
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
    AssertThrow(IsValid());

    const TypeID type_id = m_hyp_class->GetTypeID();

    if (m_hyp_class->UseHandles()) {
        HypObjectBase *hyp_object_ptr = static_cast<HypObjectBase *>(m_ptr);

        return hyp_object_ptr->GetObjectHeader_Internal()->ref_count_strong.Get(MemoryOrder::ACQUIRE);
    } else if (m_hyp_class->UseRefCountedPtr()) {
        auto *ref_count_data = static_cast<EnableRefCountedPtrFromThisBase<> *>(m_ptr)->weak.GetRefCountData_Internal();
        AssertThrow(ref_count_data != nullptr);

        return ref_count_data->UseCount_Strong();
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }
}

HYP_API void HypObjectPtr::IncRef(bool weak)
{
    AssertThrow(IsValid());

    const TypeID type_id = m_hyp_class->GetTypeID();

    if (m_hyp_class->UseHandles()) {
        HypObjectBase *hyp_object_ptr = static_cast<HypObjectBase *>(m_ptr);

        if (weak) {
            hyp_object_ptr->GetObjectHeader_Internal()->IncRefWeak();
        } else {
            hyp_object_ptr->GetObjectHeader_Internal()->IncRefStrong();
        }
    } else if (m_hyp_class->UseRefCountedPtr()) {
        EnableRefCountedPtrFromThisBase<> *ptr_casted = static_cast<EnableRefCountedPtrFromThisBase<> *>(m_ptr);
        
        auto *ref_count_data = ptr_casted->weak.GetRefCountData_Internal();
        AssertThrow(ref_count_data != nullptr);

        if (weak) {
            ref_count_data->IncRefCount_Weak();
        } else {
            ref_count_data->IncRefCount_Strong();
        }
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }
}

HYP_API void HypObjectPtr::DecRef(bool weak)
{
    AssertThrow(IsValid());

    const TypeID type_id = m_hyp_class->GetTypeID();

    if (m_hyp_class->UseHandles()) {
        HypObjectBase *hyp_object_ptr = static_cast<HypObjectBase *>(m_ptr);
        
        if (weak) {
            hyp_object_ptr->GetObjectHeader_Internal()->DecRefWeak();
        } else {
            hyp_object_ptr->GetObjectHeader_Internal()->DecRefStrong();
        }
    } else if (m_hyp_class->UseRefCountedPtr()) {
        auto *ref_count_data = static_cast<EnableRefCountedPtrFromThisBase<> *>(m_ptr)->weak.GetRefCountData_Internal();

        if (weak) {
            Weak<void> weak_ref;
            weak_ref.SetRefCountData_Internal(ref_count_data, /* inc_ref */ false);
        } else {
            RC<void> ref;
            ref.SetRefCountData_Internal(ref_count_data, /* inc_ref */ false);
        }
    } else {
        HYP_FAIL("Unhandled HypClass allocation method");
    }
}

HYP_API const IHypObjectInitializer *HypObjectPtr::GetObjectInitializer() const
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

    Stack<HypObjectInitializerContext> &contexts = g_contexts;

    bool valid = false;

    for (int i = int(contexts.Size()) - 1; i >= 0; i--) {
        HypObjectInitializerContext *context = contexts.Data() + i;

        if (context->address != address) {
            valid = false;

            break;
        }

        if (context->hyp_class == hyp_class) {
            valid = true;

            break;
        }
    }

    if (!valid) {
        ANSIString initializer_contexts_string = "\tHypClass\t\tObject Address\n";

        for (int i = int(contexts.Size()) - 1; i >= 0; i--) {
            HypObjectInitializerContext *context = contexts.Data() + i;

            initializer_contexts_string += HYP_FORMAT("\t{}\t\t{}\n", hyp_class->GetName(), context->address);
        }

        HYP_LOG(Object, Error, "Initialization context stack:\n{}", initializer_contexts_string);

        HYP_FAIL("HypObject \"%s\" being initialized incorrectly -- must be initialized using CreateObject<T> if the object is using Handle<T>, or RC<T>::Construct / MakeRefCountedPtr otherwise!",
            hyp_class->GetName().LookupString());
    }
}

HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *native_address, TypeID type_id, const HypClass *hyp_class, dotnet::Object *managed_object)
{
    AssertThrow(initializer != nullptr);
    AssertThrowMsg(hyp_class != nullptr, "No HypClass registered for class!");

    if (hyp_class->GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR) {
        // Hack to make EnableRefCountedPtr<Base> internally have TypeID of most derived class for a given instance.
        static_cast<EnableRefCountedPtrFromThisBase<> *>(native_address)->weak.GetRefCountData_Internal()->type_id = type_id;
    }

    // Fixup the pointers on objects up the chain - all should point to the HypObjectInitializer for the most derived class.
    IHypObjectInitializer *parent_initializer = initializer;

    do {
        if (const HypClass *parent_hyp_class = parent_initializer->GetClass()->GetParent()) {
            parent_initializer = parent_hyp_class->GetObjectInitializer(native_address);
            AssertThrow(parent_initializer != nullptr);

            parent_initializer->FixupPointer(native_address, initializer);
        } else {
            parent_initializer = nullptr;
        }
    } while (parent_initializer != nullptr);

    // Managed object only needs to live on the most derived class
    if (managed_object != nullptr) {
        initializer->SetManagedObject(managed_object);

        // sanity check
        dotnet::ObjectReference tmp;
        AssertThrow(initializer->GetClass()->GetManagedObject(native_address, tmp));
    }
}

HYP_API void CleanupHypObjectInitializer(const HypClass *hyp_class, dotnet::Object *managed_object_ptr)
{
    if (managed_object_ptr != nullptr) {
        delete managed_object_ptr;
    }
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

HYP_API void HypObject_OnIncRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (const IHypObjectInitializer *initializer = ptr.GetObjectInitializer()) {
        if (dotnet::Object *object = initializer->GetManagedObject()) {
            if (count == 2) {
                AssertThrow(object->SetKeepAlive(true));
            }
        }
    }
}

HYP_API void HypObject_OnDecRefCount_Strong(HypObjectPtr ptr, uint32 count)
{
    if (const IHypObjectInitializer *initializer = ptr.GetObjectInitializer()) {
        if (dotnet::Object *object = initializer->GetManagedObject()) {
            if (count == 1) {
                AssertThrow(object->SetKeepAlive(false));
            }
        }
    }
}

} // namespace hyperion