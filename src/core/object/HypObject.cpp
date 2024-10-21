/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypClassRegistry.hpp>

#include <core/containers/Stack.hpp>

#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>

namespace hyperion {

// @TODO Thread local / static initializer stack for each HypObjectInitializer
// first call for the type (base) initializes a pointer (maybe from a table or inline?)
// subsequent calls will see the ptr already exists and update it..

// maybe before constructor gets called we set the actual target HypClass ptr on a stack somewhere
// and we don't use NewObject until that matches?

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
            // HYP_LOG(Object, LogLevel::DEBUG, "Push hypclass {}, address {}", hyp_classes.Front()->GetName(), address);
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

    UniquePtr<dotnet::Object> managed_object;

    if (!(GetCurrentHypObjectInitializerFlags() & HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION)) {
        if (dotnet::Class *managed_class = hyp_class->GetManagedClass()) {
            // HYP_LOG(Object, LogLevel::DEBUG, "Create new managed {} for address {}", hyp_class->GetName(), address);

            managed_object = managed_class->NewObject(hyp_class, address);
        }
    }

    InitHypObjectInitializer(initializer, address, hyp_class->GetTypeID(), hyp_class, std::move(managed_object));

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

HYP_API void CheckHypObjectInitializer(const IHypObjectInitializer *initializer, TypeID type_id, const HypClass *hyp_class, const void *address)
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(initializer != nullptr);
    AssertThrow(hyp_class != nullptr);
    AssertThrow(address != nullptr);
#endif

    bool valid = false;

    for (int i = int(g_contexts.Size()) - 1; i >= 0; i--) {
        HypObjectInitializerContext *context = g_contexts.Data() + i;

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

        for (int i = int(g_contexts.Size()) - 1; i >= 0; i--) {
            HypObjectInitializerContext *context = g_contexts.Data() + i;

            initializer_contexts_string += HYP_FORMAT("\t{}\t\t{}\n", hyp_class->GetName(), context->address);
        }

        HYP_LOG(Object, LogLevel::ERR, "Initialization context stack:\n{}", initializer_contexts_string);

        HYP_FAIL("HypObject \"%s\" being initialized incorrectly -- must be initialized using CreateObject<T> if the object is using Handle<T>, or RC<T>::Construct / MakeRefCountedPtr otherwise!",
            hyp_class->GetName().LookupString());
    }
}

HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *native_address, TypeID type_id, const HypClass *hyp_class, UniquePtr<dotnet::Object> &&managed_object)
{
    AssertThrow(initializer != nullptr);
    AssertThrowMsg(hyp_class != nullptr, "No HypClass registered for class! Is HYP_DEFINE_CLASS missing for the type?");

    AssertThrowMsg(!hyp_class->IsAbstract(), "Cannot directly create an instance of object with HypClass \"%s\" which is marked abstract!", hyp_class->GetName().LookupString());

    if (hyp_class->GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR) {
        // Hack to make EnableRefCountedPtr<Base> internally have TypeID of most derived class for a given instance.
        static_cast<EnableRefCountedPtrFromThisBase<> *>(native_address)->weak.GetRefCountData_Internal()->type_id = type_id;
    }

    // Fixup the pointers on objects up the chain - all should point to the HypObjectInitializer for the most derived class.
    IHypObjectInitializer *parent_initializer = initializer;

    do {
        if (const HypClass *parent_hyp_class = parent_initializer->GetClass()->GetParent()) {
            if ((parent_initializer = parent_hyp_class->GetObjectInitializer(native_address))) {
                parent_initializer->FixupPointer(native_address, initializer);
            }
        } else {
            parent_initializer = nullptr;
        }
    } while (parent_initializer != nullptr);

    // Managed object only needs to live on the most derived class
    if (dotnet::Object *managed_object_ptr = managed_object.Release()) {
        initializer->SetManagedObject(managed_object_ptr);

        dotnet::ObjectReference tmp;
        AssertThrow(initializer->GetClass()->GetManagedObject(native_address, tmp));
    }
}

HYP_API void CleanupHypObjectInitializer(const HypClass *hyp_class, dotnet::Object *managed_object_ptr)
{
    // No cleanup on the managed object needed if we have a parent class - parent class will manage it
    if (managed_object_ptr != nullptr) {// && hyp_class->GetParent() == nullptr) {
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

} // namespace hyperion