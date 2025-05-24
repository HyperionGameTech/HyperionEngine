#include <core/object/managed/ManagedObjectResource.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>

#include <dotnet/Object.hpp>
#include <dotnet/Class.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Resource);
HYP_DECLARE_LOG_CHANNEL(Object);

static dotnet::Class *GetManagedClassForHypClass(const HypClass *hyp_class)
{
    dotnet::Class *managed_class = nullptr;

    while (managed_class == nullptr && hyp_class != nullptr) {
        if ((managed_class = hyp_class->GetManagedClass()) != nullptr) {
            // Cannot create an instance of an abstract class, return null
            if (managed_class->GetFlags() & ManagedClassFlags::ABSTRACT) {
                return nullptr;
            }

            break;
        }

        hyp_class = hyp_class->GetParent();
    }

    return managed_class;
}

ManagedObjectResource::ManagedObjectResource(dotnet::Object *object_ptr)
    : m_object_ptr(object_ptr)
{
}

ManagedObjectResource::ManagedObjectResource(HypObjectPtr ptr)
    : ManagedObjectResource(ptr, {}, ObjectFlags::NONE)
{
}

ManagedObjectResource::ManagedObjectResource(HypObjectPtr ptr, const dotnet::ObjectReference &object_reference, EnumFlags<ObjectFlags> object_flags)
    : m_ptr(ptr),
      m_object_ptr(nullptr)
{
    if (m_ptr) {
        IHypObjectInitializer *initializer = m_ptr.GetObjectInitializer();
        AssertDebug(initializer != nullptr);

        const HypClass *hyp_class = initializer->GetClass();
        dotnet::Class *managed_class = initializer->GetManagedClass();

        if (managed_class) {
            void *address = m_ptr.GetPointer();

            if (object_flags & ObjectFlags::CREATED_FROM_MANAGED) {
                m_object_ptr = new dotnet::Object(managed_class->RefCountedPtrFromThis(), object_reference, ObjectFlags::CREATED_FROM_MANAGED);
            } else {
                if (hyp_class->IsReferenceCounted()) {
                    // Increment reference count for the managed object (creating from managed does this already via HypObject_Initialize())
                    // The managed object is responsible for decrementing the ref count using HypObject_DecRef() in finalizer and Dispose().
                    initializer->IncRef(hyp_class->GetAllocationMethod(), address, /* weak */ false);
                }

                m_object_ptr = managed_class->NewObject(hyp_class, address);
            }

            AssertDebug(m_object_ptr != nullptr);
        }
    }
}

ManagedObjectResource::ManagedObjectResource(ManagedObjectResource &&other) noexcept
    : ResourceBase(static_cast<ResourceBase &&>(other)),
      m_ptr(other.m_ptr),
      m_object_ptr(other.m_object_ptr)
{
    other.m_ptr = nullptr;
    other.m_object_ptr = nullptr;
}

ManagedObjectResource::~ManagedObjectResource()
{
    if (m_object_ptr) {
        delete m_object_ptr;
        m_object_ptr = nullptr;
    }
}

dotnet::Class *ManagedObjectResource::GetManagedClass() const
{
    if (m_object_ptr) {
        return m_object_ptr->GetClass();
    }

    if (m_ptr.IsValid()) {
        if (const HypClass *hyp_class = m_ptr.GetClass()) {
            return hyp_class->GetManagedClass();
        }
    }

    return nullptr;
}

void ManagedObjectResource::Initialize()
{
    if (!m_object_ptr) {
        return;
    }

    if (m_object_ptr->SetKeepAlive(true)) {
        return;
    }

    if (!m_ptr.IsValid()) {
        HYP_LOG(Object, Error, "Thread: {}\tManaged object could not be kept alive, it may have been garbage collected",
            Threads::CurrentThreadID().GetName());

        return;
    }

    // Need to recreate the managed object; could be queued for finalization.
    // In this case, the ref count will be decremented once the queued object is finalized
    const HypClass *hyp_class = m_ptr.GetClass();

    HYP_LOG(Object, Info, "Thread: {}\tManaged object for object with HypClass {} at address {} could not be kept alive, it may have been garbage collected. The managed object will be recreated.",
        Threads::CurrentThreadID().GetName(),
        hyp_class->GetName(), m_ptr.GetPointer());

    if (dotnet::Class *managed_class = hyp_class->GetManagedClass()) {
        if (hyp_class->IsReferenceCounted()) {
            m_ptr.IncRef(false);
        }

        dotnet::Object *new_managed_object = managed_class->NewObject(hyp_class, m_ptr.GetPointer());

        if (!new_managed_object) {
            HYP_FAIL("Failed to recreate managed object for HypClass %s", hyp_class->GetName().LookupString());
        }

        delete m_object_ptr;

        m_object_ptr = new_managed_object;
    } else {
        HYP_FAIL("Failed to recreate managed object for HypClass %s", hyp_class->GetName().LookupString());
    }
}

void ManagedObjectResource::Destroy()
{
    if (m_object_ptr) {
        AssertThrow(m_object_ptr->SetKeepAlive(false));
    }
}

void ManagedObjectResource::Update()
{
}

} // namespace hyperion