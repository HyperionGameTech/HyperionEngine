/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_HPP
#define HYPERION_CORE_HYP_CLASS_HPP

#include <core/object/HypClassRegistry.hpp>
#include <core/object/HypObjectEnums.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypMemberFwd.hpp>
#include <core/Base.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Span.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/fbom/FBOMResult.hpp>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
struct ObjectReference;
} // namespace dotnet

namespace fbom {
class FBOMData;
} // namespace fbom

struct HypMember;
struct HypProperty;
struct HypMethod;
struct HypField;

class IHypObjectInitializer;

enum class HypClassSerializationMode : uint8
{
    NONE                        = 0x0,

    MEMBERWISE                  = 0x1,  // Use HypClassInstanceMarshal - Serialize members
    BITWISE                     = 0x2,  // Use HypClassInstanceMarshal - Serialize as FBOMStruct (binary)

    USE_MARSHAL_CLASS           = 0x80, // Use Marshal class as override

    DEFAULT = MEMBERWISE | USE_MARSHAL_CLASS
};

HYP_MAKE_ENUM_FLAGS(HypClassSerializationMode)

class HypClassMemberIterator
{
    enum class Phase
    {
        ITERATE_PROPERTIES,
        ITERATE_METHODS,
        ITERATE_FIELDS,
        MAX
    };

    friend class HypClassMemberList;

    static Phase NextPhase(EnumFlags<HypMemberType> allowed_types, Phase current)
    {
        const auto GetNext = [](Phase phase) -> Phase
        {
            if (phase == Phase::MAX) {
                return Phase::ITERATE_PROPERTIES;
            }

            return static_cast<Phase>(static_cast<int>(phase) + 1);
        };

        const auto CanDoNext = [allowed_types](Phase next_phase) -> bool
        {
            switch (next_phase) {
            case Phase::ITERATE_PROPERTIES:
                return allowed_types & HypMemberType::TYPE_PROPERTY;
            case Phase::ITERATE_METHODS:
                return allowed_types & HypMemberType::TYPE_METHOD;
            case Phase::ITERATE_FIELDS:
                return allowed_types & HypMemberType::TYPE_FIELD;
            default:
                return true;
            }
        };

        Phase next_phase = GetNext(current);

        while (!CanDoNext(next_phase)) {
            next_phase = GetNext(next_phase);
        }

        return next_phase;
    }

    HYP_API HypClassMemberIterator(const HypClass *hyp_class, EnumFlags<HypMemberType> member_types, Phase phase);

public:
    HypClassMemberIterator(const HypClassMemberIterator &other)
        : m_member_types(other.m_member_types),
          m_phase(other.m_phase),
          m_hyp_class(other.m_hyp_class),
          m_iterating_parent(other.m_iterating_parent),
          m_current_index(other.m_current_index),
          m_current_value(other.m_current_value)
    {
    }

    HypClassMemberIterator &operator=(const HypClassMemberIterator &other)
    {
        if (this == &other) {
            return *this;
        }

        m_member_types = other.m_member_types;
        m_phase = other.m_phase;
        m_hyp_class = other.m_hyp_class;
        m_iterating_parent = other.m_iterating_parent;
        m_current_index = other.m_current_index;
        m_current_value = other.m_current_value;

        return *this;
    }

    HYP_FORCE_INLINE bool operator==(const HypClassMemberIterator &other) const
    {
        return m_member_types == other.m_member_types
            && m_phase == other.m_phase
            && m_hyp_class == other.m_hyp_class
            && m_iterating_parent == other.m_iterating_parent
            && m_current_index == other.m_current_index
            && m_current_value == other.m_current_value;
    }

    HYP_FORCE_INLINE bool operator!=(const HypClassMemberIterator &other) const
    {
        return m_member_types != other.m_member_types
            || m_phase != other.m_phase
            || m_hyp_class != other.m_hyp_class
            || m_iterating_parent != other.m_iterating_parent
            || m_current_index != other.m_current_index
            || m_current_value != other.m_current_value;
    }

    HYP_FORCE_INLINE HypClassMemberIterator &operator++()
    {
        Advance();

        return *this;
    }

    HypClassMemberIterator operator++(int)
    {
        HypClassMemberIterator result(*this);
        ++result;
        return result;
    }

    HYP_FORCE_INLINE IHypMember &operator*()
    {
        return *m_current_value;
    }

    HYP_FORCE_INLINE const IHypMember &operator*() const
    {
        return *m_current_value;
    }

    HYP_FORCE_INLINE IHypMember *operator->()
    {
        return m_current_value;
    }

    HYP_FORCE_INLINE const IHypMember *operator->() const
    {
        return m_current_value;
    }

private:
    HYP_API void Advance();

    EnumFlags<HypMemberType>    m_member_types;

    Phase                       m_phase;
    const HypClass              *m_hyp_class;
    const HypClass              *m_iterating_parent;
    SizeType                    m_current_index;

    mutable IHypMember          *m_current_value;
};

class HypClassMemberList
{
public:
    using Iterator = HypClassMemberIterator;
    using ConstIterator = HypClassMemberIterator;

    HypClassMemberList(const HypClass *hyp_class, EnumFlags<HypMemberType> member_types)
        : m_hyp_class(hyp_class),
          m_member_types(member_types)
    {
    }

    HypClassMemberList(const HypClassMemberList &other)                 = default;
    HypClassMemberList &operator=(const HypClassMemberList &other)      = default;

    HypClassMemberList(HypClassMemberList &&other) noexcept             = default;
    HypClassMemberList &operator=(HypClassMemberList &&other) noexcept  = default;

    ~HypClassMemberList()                                               = default;

    HYP_DEF_STL_BEGIN_END(HypClassMemberIterator(m_hyp_class, m_member_types, HypClassMemberIterator::Phase::ITERATE_PROPERTIES), HypClassMemberIterator(m_hyp_class, m_member_types, HypClassMemberIterator::Phase::MAX))

private:
    const HypClass              *m_hyp_class;
    EnumFlags<HypMemberType>    m_member_types;
};

class HYP_API HypClass
{
public:
    friend struct detail::HypClassRegistrationBase;

    HypClass(TypeID type_id, Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members);
    HypClass(const HypClass &other)                 = delete;
    HypClass &operator=(const HypClass &other)      = delete;
    HypClass(HypClass &&other) noexcept             = delete;
    HypClass &operator=(HypClass &&other) noexcept  = delete;
    virtual ~HypClass();

    virtual void Initialize();

    virtual bool IsValid() const
        { return false; }

    virtual HypClassAllocationMethod GetAllocationMethod() const = 0;

    HYP_FORCE_INLINE bool UseHandles() const
        { return GetAllocationMethod() == HypClassAllocationMethod::OBJECT_POOL_HANDLE; }

    HYP_FORCE_INLINE bool UseRefCountedPtr() const
        { return GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR; }

    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    HYP_FORCE_INLINE const HypClass *GetParent() const
        { return m_parent; }

    virtual SizeType GetSize() const = 0;

    HYP_FORCE_INLINE IHypObjectInitializer *GetObjectInitializer(void *object_ptr) const
        { return GetObjectInitializer_Internal(object_ptr); }

    HYP_FORCE_INLINE const IHypObjectInitializer *GetObjectInitializer(const void *object_ptr) const
        { return GetObjectInitializer_Internal(const_cast<void *>(object_ptr)); }

    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    HYP_FORCE_INLINE EnumFlags<HypClassFlags> GetFlags() const
        { return m_flags; }

    HYP_FORCE_INLINE bool IsClassType() const
        { return m_flags & HypClassFlags::CLASS_TYPE; }

    HYP_FORCE_INLINE bool IsStructType() const
        { return m_flags & HypClassFlags::STRUCT_TYPE; }

    HYP_FORCE_INLINE bool IsPOD() const
        { return m_flags & HypClassFlags::POD_TYPE; }

    HYP_FORCE_INLINE bool IsAbstract() const
        { return m_flags & HypClassFlags::ABSTRACT; }

    bool CanSerialize() const;

    HYP_FORCE_INLINE EnumFlags<HypClassSerializationMode> GetSerializationMode() const
        { return m_serialization_mode; }

    HYP_FORCE_INLINE const HypClassAttributeSet &GetAttributes() const
        { return m_attributes; }

    HYP_FORCE_INLINE const HypClassAttributeValue &GetAttribute(ANSIStringView key) const
        { return m_attributes[key]; }

    HYP_FORCE_INLINE const HypClassAttributeValue &GetAttribute(ANSIStringView key, const HypClassAttributeValue &default_value) const
        { return m_attributes.Get(key, default_value); }

    HYP_FORCE_INLINE HypClassMemberList GetMembers(EnumFlags<HypMemberType> member_types) const
        { return { this, member_types }; }

    HYP_FORCE_INLINE HypClassMemberList GetMembers(bool include_properties = false) const
        { return { this, HypMemberType::TYPE_METHOD | HypMemberType::TYPE_FIELD | (include_properties ? HypMemberType::TYPE_PROPERTY : HypMemberType::NONE) }; }

    IHypMember *GetMember(WeakName name) const;

    HypProperty *GetProperty(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypProperty *> &GetProperties() const
        { return m_properties; }

    Array<HypProperty *> GetPropertiesInherited() const;

    HypMethod *GetMethod(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypMethod *> &GetMethods() const
        { return m_methods; }

    Array<HypMethod *> GetMethodsInherited() const;

    HypField *GetField(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypField *> &GetFields() const
        { return m_fields; }

    Array<HypField *> GetFieldsInherited() const;

    dotnet::Class *GetManagedClass() const;

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const = 0;

    virtual bool CanCreateInstance() const = 0;

    HYP_FORCE_INLINE void CreateInstance(HypData &out, bool allow_abstract = false) const
    {
        AssertThrowMsg(CanCreateInstance() && (allow_abstract || !IsAbstract()), "Cannot create a new instance for HypClass %s", GetName().LookupString());

        CreateInstance_Internal(out);
    }

    // HYP_FORCE_INLINE void CreateInstance(HypData &out, UniquePtr<dotnet::Object> &&managed_object) const
    // {
    //     AssertThrowMsg(CanCreateInstance() && !IsAbstract(), "Cannot create a new instance for HypClass %s", GetName().LookupString());

    //     AssertThrowMsg(managed_object != nullptr, "Managed object must not be null for this overload of CreateInstance()");

    //     CreateInstance_Internal(out, std::move(managed_object));
    // }

    HYP_FORCE_INLINE HashCode GetInstanceHashCode(ConstAnyRef ref) const
    {
        AssertThrowMsg(ref.GetTypeID() == GetTypeID(), "Expected HypClass instance to have type ID %u but got type ID %u",
            ref.GetTypeID().Value(), GetTypeID().Value());

        return GetInstanceHashCode_Internal(ref);
    }

protected:
    virtual IHypObjectInitializer *GetObjectInitializer_Internal(void *object_ptr) const = 0;

    virtual void CreateInstance_Internal(HypData &out) const = 0;
    virtual void CreateInstance_Internal(HypData &out, UniquePtr<dotnet::Object> &&managed_object) const = 0;

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const = 0;

    static bool GetManagedObjectFromObjectInitializer(const IHypObjectInitializer *object_initializer, dotnet::ObjectReference &out_object_reference);

    TypeID                                  m_type_id;
    Name                                    m_name;
    Name                                    m_parent_name;
    const HypClass                          *m_parent;
    HypClassAttributeSet                    m_attributes;
    EnumFlags<HypClassFlags>                m_flags;
    Array<HypProperty *>                    m_properties;
    HashMap<Name, HypProperty *>            m_properties_by_name;
    Array<HypMethod *>                      m_methods;
    HashMap<Name, HypMethod *>              m_methods_by_name;
    Array<HypField *>                       m_fields;
    HashMap<Name, HypField *>               m_fields_by_name;
    EnumFlags<HypClassSerializationMode>    m_serialization_mode;
};

template <class T>
class HypClassInstance final : public HypClass
{
public:
    static HypClassInstance &GetInstance(Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypClassInstance instance { name, parent_name, attributes, flags, members };

        return instance;
    }

    HypClassInstance(Name name, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(TypeID::ForType<T>(), name, parent_name, attributes, flags, members)
    {
    }

    virtual ~HypClassInstance() = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        if constexpr (std::is_base_of_v<HypObjectBase, T>) {
            return HypClassAllocationMethod::OBJECT_POOL_HANDLE;
        } else {
            static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>, "HypObject must inherit EnableRefCountedPtrFromThis<T> if it does not use ObjectPool (Handle<T>)");
            
            return HypClassAllocationMethod::REF_COUNTED_PTR;
        }
    }

    virtual SizeType GetSize() const override
    {
        return sizeof(T);
    }

    virtual bool GetManagedObject(const void *object_ptr, dotnet::ObjectReference &out_object_reference) const override
    {
        return GetManagedObjectFromObjectInitializer(HypClass::GetObjectInitializer(object_ptr), out_object_reference);
    }

    virtual bool CanCreateInstance() const override
    {
        if constexpr (std::is_default_constructible_v<T>) {
            return true;
        } else {
            return false;
        }
    }
    
protected:
    virtual IHypObjectInitializer *GetObjectInitializer_Internal(void *object_ptr) const override
    {
        if (!object_ptr) {
            return nullptr;
        }

        return static_cast<T *>(object_ptr)->GetObjectInitializer();
    }
    
    virtual void CreateInstance_Internal(HypData &out) const override
    {
        if constexpr (std::is_default_constructible_v<T>) {
            if constexpr (std::is_base_of_v<HypObjectBase, T>) {
                out = CreateObject<T>();
            } else {
                out = MakeRefCountedPtr<T>();
            }
        } else {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    virtual void CreateInstance_Internal(HypData &out, UniquePtr<dotnet::Object> &&managed_object) const override
    {
        // Suppress default managed object creation if a managed object has been passed in
        HypObjectInitializerFlagsGuard flags_guard(HypObjectInitializerFlags::SUPPRESS_MANAGED_OBJECT_CREATION);

        if constexpr (std::is_default_constructible_v<T>) {
            if constexpr (std::is_base_of_v<HypObjectBase, T>) {
                out = CreateObject<T>();
            } else {
                out = MakeRefCountedPtr<T>();
            }

            void *address = out.ToRef().GetPointer();

            IHypObjectInitializer *initializer = GetObjectInitializer(address);
            AssertThrow(initializer != nullptr);

            initializer->SetManagedObject(managed_object.Release());

            // SetHypObjectInitializerManagedObject(GetObjectInitializer(address), address, std::move(managed_object));
        } else {
            HYP_NOT_IMPLEMENTED_VOID();
        }
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        if constexpr (HasGetHashCode<T, HashCode>::value) {
            return HashCode::GetHashCode(ref.Get<T>());
        } else {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

} // namespace hyperion

#endif