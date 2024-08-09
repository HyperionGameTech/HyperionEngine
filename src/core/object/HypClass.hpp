/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_HPP
#define HYPERION_CORE_HYP_CLASS_HPP

#include <core/object/HypClassRegistry.hpp>

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
} // namespace dotnet

class HypProperty;

class HYP_API HypClass
{
public:
    friend struct detail::HypClassRegistrationBase;

    HypClass(TypeID type_id, EnumFlags<HypClassFlags> flags, Span<HypProperty> properties);
    HypClass(const HypClass &other)                 = delete;
    HypClass &operator=(const HypClass &other)      = delete;
    HypClass(HypClass &&other) noexcept             = delete;
    HypClass &operator=(HypClass &&other) noexcept  = delete;
    virtual ~HypClass();

    virtual Name GetName() const = 0;

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    HYP_FORCE_INLINE EnumFlags<HypClassFlags> GetFlags() const
        { return m_flags; }

    virtual bool IsValid() const
        { return false; }

    HypProperty *GetProperty(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypProperty *> &GetProperties() const
        { return m_properties; }

    dotnet::Class *GetManagedClass() const;

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

    TypeID                              m_type_id;
    EnumFlags<HypClassFlags>            m_flags;
    Array<HypProperty *>           m_properties;
    HashMap<Name, HypProperty *>   m_properties_by_name;
};

template <class T>
class HypClassInstance : public HypClass
{
public:
    static HypClassInstance &GetInstance(EnumFlags<HypClassFlags> flags, Span<HypProperty> properties)
    {
        static HypClassInstance instance { flags, properties };

        return instance;
    }

    HypClassInstance(EnumFlags<HypClassFlags> flags, Span<HypProperty> properties)
        : HypClass(TypeID::ForType<T>(), flags, properties)
    {
    }

    virtual ~HypClassInstance() = default;

    virtual Name GetName() const override
    {
        static const Name name = CreateNameFromDynamicString(TypeNameWithoutNamespace<T>().Data());

        return name;
    }

    virtual bool IsValid() const override
    {
        return true;
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