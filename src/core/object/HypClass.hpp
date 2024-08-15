/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_HPP
#define HYPERION_CORE_HYP_CLASS_HPP

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObjectEnums.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Span.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
struct ObjectReference;
} // namespace dotnet

struct HypMember;
struct HypProperty;
struct HypMethod;
struct HypField;

class IHypObjectInitializer;

class HYP_API HypClass
{
public:
    friend struct detail::HypClassRegistrationBase;

    HypClass(TypeID type_id, EnumFlags<HypClassFlags> flags, Span<HypMember> members);
    HypClass(const HypClass &other)                 = delete;
    HypClass &operator=(const HypClass &other)      = delete;
    HypClass(HypClass &&other) noexcept             = delete;
    HypClass &operator=(HypClass &&other) noexcept  = delete;
    virtual ~HypClass();

    virtual bool IsValid() const
        { return false; }

    virtual HypClassAllocationMethod GetAllocationMethod() const = 0;

    HYP_FORCE_INLINE bool UseHandles() const
        { return GetAllocationMethod() == HypClassAllocationMethod::OBJECT_POOL_HANDLE; }

    HYP_FORCE_INLINE bool UseRefCountedPtr() const
        { return GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR; }

    virtual Name GetName() const = 0;

    virtual SizeType GetSize() const = 0;

    virtual const IHypObjectInitializer *GetObjectInitializer(const void *object_ptr) const = 0;

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    HYP_FORCE_INLINE EnumFlags<HypClassFlags> GetFlags() const
        { return m_flags; }

    HYP_FORCE_INLINE bool IsClassType() const
        { return m_flags & HypClassFlags::CLASS_TYPE; }

    HYP_FORCE_INLINE bool IsStructType() const
        { return m_flags & HypClassFlags::STRUCT_TYPE; }

    HypProperty *GetProperty(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypProperty *> &GetProperties() const
        { return m_properties; }

    HypMethod *GetMethod(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypMethod *> &GetMethods() const
        { return m_methods; }

    HypField *GetField(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypField *> &GetFields() const
        { return m_fields; }

    dotnet::Class *GetManagedClass() const;

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const = 0;

    template <class T>
    HYP_FORCE_INLINE void CreateInstance(T *out_ptr) const
    {
        AssertThrowMsg(TypeID::ForType<T>() == GetTypeID(), "Expected HypClass instance to have type ID %u but got type ID %u",
            TypeID::ForType<T>().Value(), GetTypeID().Value());

        CreateInstance_Internal(out_ptr);
    }

    HYP_FORCE_INLINE void CreateInstance(Any &out) const
    {
        CreateInstance_Internal(out);
    }

    HYP_FORCE_INLINE HashCode GetInstanceHashCode(ConstAnyRef ref) const
    {
        AssertThrowMsg(ref.GetTypeID() == GetTypeID(), "Expected HypClass instance to have type ID %u but got type ID %u",
            ref.GetTypeID().Value(), GetTypeID().Value());

        return GetInstanceHashCode_Internal(ref);
    }

protected:
    virtual void CreateInstance_Internal(void *out_ptr) const = 0;
    virtual void CreateInstance_Internal(Any &out) const = 0;
    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const = 0;

    static bool GetManagedObjectFromObjectInitializer(const IHypObjectInitializer *object_initializer, dotnet::ObjectReference &out_object_reference);

    TypeID                          m_type_id;
    EnumFlags<HypClassFlags>        m_flags;
    Array<HypProperty *>            m_properties;
    HashMap<Name, HypProperty *>    m_properties_by_name;
    Array<HypMethod *>              m_methods;
    HashMap<Name, HypMethod *>      m_methods_by_name;
    Array<HypField *>               m_fields;
    HashMap<Name, HypField *>       m_fields_by_name;
};

template <class T>
class HypClassInstance : public HypClass
{
public:
    static HypClassInstance &GetInstance(EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypClassInstance instance { flags, members };

        return instance;
    }

    HypClassInstance(EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(TypeID::ForType<T>(), flags, members)
    {
    }

    virtual ~HypClassInstance() = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        if constexpr (has_opaque_handle_defined<T>) {
            return HypClassAllocationMethod::OBJECT_POOL_HANDLE;
        } else {
            static_assert(std::is_base_of_v<EnableRefCountedPtrFromThis<T>, T>, "HypObject must inherit EnableRefCountedPtrFromThis<T> if it does not use ObjectPool (Handle<T>)");
            return HypClassAllocationMethod::REF_COUNTED_PTR;
        }
    }

    virtual Name GetName() const override
    {
        static const Name name = CreateNameFromDynamicString(TypeNameWithoutNamespace<T>().Data());

        return name;
    }

    virtual SizeType GetSize() const override
    {
        return sizeof(T);
    }

    virtual const IHypObjectInitializer *GetObjectInitializer(const void *object_ptr) const override
    {
        if (!object_ptr) {
            return nullptr;
        }

        return &static_cast<const T *>(object_ptr)->GetObjectInitializer();
    }

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override
    {
        if (!object_ptr) {
            return false;
        }

        return GetManagedObjectFromObjectInitializer(GetObjectInitializer(object_ptr), out_object_reference);
    }

    T CreateInstance() const
    {
        ValueStorage<T> result_storage;
        CreateInstance_Internal(result_storage.GetPointer());

        return result_storage.Get();
    }

protected:
    virtual void CreateInstance_Internal(void *out_ptr) const override
        { Memory::Construct<T>(out_ptr); }

    virtual void CreateInstance_Internal(Any &out) const override
        { out.Emplace<T>(); }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
        { return HashCode::GetHashCode(ref.Get<T>()); }
};

} // namespace hyperion

#endif