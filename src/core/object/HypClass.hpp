/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_HPP
#define HYPERION_CORE_HYP_CLASS_HPP

#include <core/object/HypObjectFwd.hpp>
#include <core/object/HypObjectEnums.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypMemberFwd.hpp>
#include <core/object/HypClassAttribute.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

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

class IResource;

struct HypMember;
class HypProperty;
class HypMethod;
class HypField;
class HypConstant;
class HypClass;
class HypEnum;
class HypStruct;

template <class T>
class HypClassInstance;

template <class T>
class HypStructInstance;

template <class T>
class HypEnumInstance;

class IHypObjectInitializer;

enum class HypClassFlags : uint32
{
    NONE = 0x0,
    CLASS_TYPE = 0x1,
    STRUCT_TYPE = 0x2,
    ENUM_TYPE = 0x4,
    ABSTRACT = 0x8,
    POD_TYPE = 0x10,
    DYNAMIC = 0x20 // Dynamic classes are not registered in the class registry
};

HYP_MAKE_ENUM_FLAGS(HypClassFlags)

enum class HypClassSerializationMode : uint8
{
    NONE = 0x0,

    MEMBERWISE = 0x1, // Use HypClassInstanceMarshal - Serialize members
    BITWISE = 0x2,    // Use HypClassInstanceMarshal - Serialize as FBOMStruct (binary)

    USE_MARSHAL_CLASS = 0x80, // Use Marshal class as override

    DEFAULT = MEMBERWISE | USE_MARSHAL_CLASS
};

HYP_MAKE_ENUM_FLAGS(HypClassSerializationMode)

#pragma region Helpers

HYP_API const HypClass* GetClass(TypeID type_id);
HYP_API const HypClass* GetClass(WeakName type_name);
HYP_API const HypEnum* GetEnum(TypeID type_id);
HYP_API const HypEnum* GetEnum(WeakName type_name);

HYP_API SizeType GetNumDescendants(TypeID type_id);

template <class T>
static inline SizeType GetNumDescendants()
{
    return GetNumDescendants(TypeID::ForType<T>());
}

HYP_API int GetSubclassIndex(TypeID base_type_id, TypeID subclass_type_id);

template <class T, class U>
static inline int GetSubclassIndex()
{
    static const int idx = GetSubclassIndex(TypeID::ForType<T>(), TypeID::ForType<U>());

    return idx;
}

template <class T>
HYP_FORCE_INLINE const HypClass* GetClass()
{
    return GetClass(TypeID::ForType<T>());
}

template <class T>
HYP_FORCE_INLINE const HypEnumInstance<T>* GetEnum()
{
    return static_cast<const HypEnumInstance<T>*>(GetEnum(TypeID::ForType<T>()));
}

HYP_API bool IsInstanceOfHypClass(const HypClass* hyp_class, const void* ptr, TypeID type_id);
HYP_API bool IsInstanceOfHypClass(const HypClass* hyp_class, const HypClass* instance_hyp_class);

#pragma endregion Helpers

class HypClassMemberIterator
{
    enum class Phase
    {
        ITERATE_CONSTANTS,
        ITERATE_PROPERTIES,
        ITERATE_METHODS,
        ITERATE_FIELDS,
        MAX
    };

    friend class HypClassMemberList;

    static Phase NextPhase(EnumFlags<HypMemberType> allowed_types, Phase current)
    {
        const auto get_next = [](Phase phase) -> Phase
        {
            if (phase == Phase::MAX)
            {
                return Phase::ITERATE_CONSTANTS;
            }

            return static_cast<Phase>(static_cast<int>(phase) + 1);
        };

        const auto can_do_next = [allowed_types](Phase next_phase) -> bool
        {
            switch (next_phase)
            {
            case Phase::ITERATE_CONSTANTS:
                return allowed_types & HypMemberType::TYPE_CONSTANT;
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

        Phase next_phase = get_next(current);

        while (!can_do_next(next_phase))
        {
            next_phase = get_next(next_phase);
        }

        return next_phase;
    }

    HYP_API HypClassMemberIterator(const HypClass* hyp_class, EnumFlags<HypMemberType> member_types, Phase phase);

public:
    HypClassMemberIterator(const HypClassMemberIterator& other)
        : m_member_types(other.m_member_types),
          m_phase(other.m_phase),
          m_target(other.m_target),
          m_current_index(other.m_current_index),
          m_current_value(other.m_current_value)
    {
    }

    HypClassMemberIterator& operator=(const HypClassMemberIterator& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_member_types = other.m_member_types;
        m_phase = other.m_phase;
        m_target = other.m_target;
        m_current_index = other.m_current_index;
        m_current_value = other.m_current_value;

        return *this;
    }

    HYP_FORCE_INLINE bool operator==(const HypClassMemberIterator& other) const
    {
        return m_member_types == other.m_member_types
            && m_phase == other.m_phase
            && m_target == other.m_target
            && m_current_index == other.m_current_index
            && m_current_value == other.m_current_value;
    }

    HYP_FORCE_INLINE bool operator!=(const HypClassMemberIterator& other) const
    {
        return m_member_types != other.m_member_types
            || m_phase != other.m_phase
            || m_target != other.m_target
            || m_current_index != other.m_current_index
            || m_current_value != other.m_current_value;
    }

    HYP_FORCE_INLINE HypClassMemberIterator& operator++()
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

    HYP_FORCE_INLINE IHypMember& operator*()
    {
        return *m_current_value;
    }

    HYP_FORCE_INLINE const IHypMember& operator*() const
    {
        return *m_current_value;
    }

    HYP_FORCE_INLINE IHypMember* operator->()
    {
        return m_current_value;
    }

    HYP_FORCE_INLINE const IHypMember* operator->() const
    {
        return m_current_value;
    }

private:
    HYP_API void Advance();

    EnumFlags<HypMemberType> m_member_types;

    Phase m_phase;
    const HypClass* m_target;
    SizeType m_current_index;

    mutable IHypMember* m_current_value;
};

class HypClassMemberList
{
public:
    using Iterator = HypClassMemberIterator;
    using ConstIterator = HypClassMemberIterator;

    HypClassMemberList(const HypClass* hyp_class, EnumFlags<HypMemberType> member_types)
        : m_hyp_class(hyp_class),
          m_member_types(member_types)
    {
    }

    HypClassMemberList(const HypClassMemberList& other) = default;
    HypClassMemberList& operator=(const HypClassMemberList& other) = default;

    HypClassMemberList(HypClassMemberList&& other) noexcept = default;
    HypClassMemberList& operator=(HypClassMemberList&& other) noexcept = default;

    ~HypClassMemberList() = default;

    HYP_DEF_STL_BEGIN_END(HypClassMemberIterator(m_hyp_class, m_member_types, HypClassMemberIterator::Phase::ITERATE_PROPERTIES), HypClassMemberIterator(nullptr, m_member_types, HypClassMemberIterator::Phase::MAX))

private:
    const HypClass* m_hyp_class;
    EnumFlags<HypMemberType> m_member_types;
};

enum class HypClassCallbackType
{
    ON_POST_LOAD = 0
};

class IHypClassCallbackWrapper
{
public:
    virtual ~IHypClassCallbackWrapper() = default;
};

template <class Callback>
class HypClassCallbackWrapper : public IHypClassCallbackWrapper
{
public:
    HypClassCallbackWrapper(Callback&& callback)
        : m_callback(std::forward<Callback>(callback))
    {
    }

    HYP_FORCE_INLINE Callback GetCallback() const
    {
        return m_callback;
    }

private:
    Callback m_callback;
};

template <HypClassCallbackType CallbackType>
class HypClassCallbackCollection
{
public:
    static HypClassCallbackCollection& GetInstance()
    {
        static HypClassCallbackCollection instance;
        return instance;
    }

    const IHypClassCallbackWrapper* GetCallback(const TypeID& type_id) const
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_callbacks.Find(type_id);

        if (it != m_callbacks.End())
        {
            return it->second;
        }

        return nullptr;
    }

    void SetCallback(const TypeID& type_id, const IHypClassCallbackWrapper* callback)
    {
        Mutex::Guard guard(m_mutex);

        m_callbacks[type_id] = callback;
    }

private:
    HashMap<TypeID, const IHypClassCallbackWrapper*> m_callbacks;
    mutable Mutex m_mutex;
};

template <HypClassCallbackType CallbackType>
struct HypClassCallbackRegistration
{
    template <auto Callback>
    HypClassCallbackRegistration(const TypeID& type_id, ValueWrapper<Callback>)
    {
        static const HypClassCallbackWrapper callback_wrapper(Callback);

        HypClassCallbackCollection<CallbackType>::GetInstance().SetCallback(type_id, &callback_wrapper);
    }
};

class HYP_API HypClass
{
public:
    friend struct HypClassRegistrationBase;

    HypClass(TypeID type_id, Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members);
    HypClass(const HypClass& other) = delete;
    HypClass& operator=(const HypClass& other) = delete;
    HypClass(HypClass&& other) noexcept = delete;
    HypClass& operator=(HypClass&& other) noexcept = delete;
    virtual ~HypClass();

    virtual void Initialize();

    virtual bool IsValid() const
    {
        return false;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const = 0;

    HYP_FORCE_INLINE bool UseHandles() const
    {
        return GetAllocationMethod() == HypClassAllocationMethod::HANDLE;
    }

    HYP_FORCE_INLINE bool UseRefCountedPtr() const
    {
        return GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR;
    }

    HYP_FORCE_INLINE bool IsReferenceCounted() const
    {
        return GetAllocationMethod() == HypClassAllocationMethod::HANDLE
            || GetAllocationMethod() == HypClassAllocationMethod::REF_COUNTED_PTR;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    /*! \brief Returns the statically assigned index of this class in the global HypClass table.
     *  Only classes that are picked up by the build tool at configuration time will have a static index assigned.
     *  (Dynamic types created in managed code will return -1 for this)
     *  \note GetStaticIndex is used mainly for fast type checking internally, as it can be used to compare with HypClass::GetNumDescendants to check
     *  if the static index is within the expected range. It can also be used to preallocate slots for subclasses (see ObjectBinder<T> for example usage) */
    HYP_FORCE_INLINE int GetStaticIndex() const
    {
        return m_static_index;
    }

    /*! \brief Returns the total number of descendants of this HypClass. */
    HYP_FORCE_INLINE uint32 GetNumDescendants() const
    {
        return m_num_descendants;
    }

    HYP_FORCE_INLINE const HypClass* GetParent() const
    {
        return m_parent;
    }

    /*! \brief Check if this HypClass is derived from the given HypClass or is equal to it.
     *  \param other The HypClass to check this against
     *  \return True if this HypClass is derived from or equal to the given HypClass, false otherwise. */
    bool IsDerivedFrom(const HypClass* other) const;

    HYP_FORCE_INLINE SizeType GetSize() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE SizeType GetAlignment() const
    {
        return m_alignment;
    }

    HYP_FORCE_INLINE IHypObjectInitializer* GetObjectInitializer(void* object_ptr) const
    {
        return GetObjectInitializer_Internal(object_ptr);
    }

    HYP_FORCE_INLINE const IHypObjectInitializer* GetObjectInitializer(const void* object_ptr) const
    {
        return GetObjectInitializer_Internal(const_cast<void*>(object_ptr));
    }

    virtual void FixupPointer(void* target, IHypObjectInitializer* new_initializer) const = 0;

    HYP_FORCE_INLINE TypeID GetTypeID() const
    {
        return m_type_id;
    }

    HYP_FORCE_INLINE EnumFlags<HypClassFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE bool IsClassType() const
    {
        return m_flags & HypClassFlags::CLASS_TYPE;
    }

    HYP_FORCE_INLINE bool IsStructType() const
    {
        return m_flags & HypClassFlags::STRUCT_TYPE;
    }

    HYP_FORCE_INLINE bool IsEnumType() const
    {
        return m_flags & HypClassFlags::ENUM_TYPE;
    }

    HYP_FORCE_INLINE bool IsPOD() const
    {
        return m_flags & HypClassFlags::POD_TYPE;
    }

    HYP_FORCE_INLINE bool IsAbstract() const
    {
        return m_flags & HypClassFlags::ABSTRACT;
    }

    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return m_flags & HypClassFlags::DYNAMIC;
    }

    bool CanSerialize() const;

    HYP_FORCE_INLINE EnumFlags<HypClassSerializationMode> GetSerializationMode() const
    {
        return m_serialization_mode;
    }

    HYP_FORCE_INLINE const HypClassAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE const HypClassAttributeValue& GetAttribute(ANSIStringView key) const
    {
        return m_attributes[key];
    }

    HYP_FORCE_INLINE const HypClassAttributeValue& GetAttribute(ANSIStringView key, const HypClassAttributeValue& default_value) const
    {
        return m_attributes.Get(key, default_value);
    }

    HYP_FORCE_INLINE HypClassMemberList GetMembers(EnumFlags<HypMemberType> member_types) const
    {
        return { this, member_types };
    }

    HYP_FORCE_INLINE HypClassMemberList GetMembers(bool include_properties = false) const
    {
        return { this, HypMemberType::TYPE_METHOD | HypMemberType::TYPE_FIELD | HypMemberType::TYPE_CONSTANT | (include_properties ? HypMemberType::TYPE_PROPERTY : HypMemberType::NONE) };
    }

    IHypMember* GetMember(WeakName name) const;

    HypProperty* GetProperty(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypProperty*>& GetProperties() const
    {
        return m_properties;
    }

    Array<HypProperty*> GetPropertiesInherited() const;

    HypMethod* GetMethod(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypMethod*>& GetMethods() const
    {
        return m_methods;
    }

    Array<HypMethod*> GetMethodsInherited() const;

    HypField* GetField(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypField*>& GetFields() const
    {
        return m_fields;
    }

    Array<HypField*> GetFieldsInherited() const;

    HypConstant* GetConstant(WeakName name) const;

    HYP_FORCE_INLINE const Array<HypConstant*>& GetConstants() const
    {
        return m_constants;
    }

    Array<HypConstant*> GetConstantsInherited() const;

    HYP_FORCE_INLINE RC<dotnet::Class> GetManagedClass() const
    {
        Mutex::Guard guard(m_managed_class_mutex);

        return m_managed_class.Lock();
    }

    HYP_FORCE_INLINE void SetManagedClass(const RC<dotnet::Class>& managed_class) const
    {
        Mutex::Guard guard(m_managed_class_mutex);

        m_managed_class = managed_class;
    }

    virtual bool GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const = 0;

    virtual bool CanCreateInstance() const = 0;

    HYP_FORCE_INLINE bool CreateInstance(HypData& out, bool allow_abstract = false) const
    {
        AssertThrowMsg(CanCreateInstance() && (allow_abstract || !IsAbstract()), "Cannot create a new instance for HypClass %s!\n\tCanCreateInstance: %s\tIsAbstract: %s\tAllow abstract: %s",
            GetName().LookupString(), CanCreateInstance() ? "true" : "false", IsAbstract() ? "true" : "false", allow_abstract ? "true" : "false");

        return CreateInstance_Internal(out);
    }

    HYP_FORCE_INLINE bool CreateInstanceArray(Span<HypData> elements, HypData& out, bool allow_abstract = false) const
    {
        AssertThrowMsg(CanCreateInstance() && (allow_abstract || !IsAbstract()), "Cannot create a new instance for HypClass %s!\n\tCanCreateInstance: %s\tIsAbstract: %s\tAllow abstract: %s",
            GetName().LookupString(), CanCreateInstance() ? "true" : "false", IsAbstract() ? "true" : "false", allow_abstract ? "true" : "false");

        return CreateInstanceArray_Internal(elements, out);
    }

    /*! \brief Create a new HypData from \ref{memory}. The object at \ref{memory} must have the type of this HypClass's TypeID.
     *  The underlying data will be moved or have ownership taken.
     *  \param memory A view to the memory of the underlying object.
     *  \returns True if the operation was successful. */
    virtual bool ToHypData(ByteView memory, HypData& out_hyp_data) const
    {
        return false;
    }

    HYP_FORCE_INLINE HashCode GetInstanceHashCode(ConstAnyRef ref) const
    {
        AssertThrowMsg(ref.GetTypeID() == GetTypeID(), "Expected HypClass instance to have type ID %u but got type ID %u",
            ref.GetTypeID().Value(), GetTypeID().Value());

        return GetInstanceHashCode_Internal(ref);
    }

    void PostLoad(void* object_ptr) const
    {
        if (!object_ptr)
        {
            return;
        }

        const HypClass* hyp_class = this;

        while (hyp_class)
        {
            hyp_class->PostLoad_Internal(object_ptr);

            hyp_class = hyp_class->GetParent();
        }
    }

protected:
    virtual void PostLoad_Internal(void* object_ptr) const
    {
    }

    virtual IHypObjectInitializer* GetObjectInitializer_Internal(void* object_ptr) const = 0;

    virtual bool CreateInstance_Internal(HypData& out) const
    {
        return false;
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const
    {
        return false;
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const = 0;

    static bool GetManagedObjectFromObjectInitializer(const IHypObjectInitializer* object_initializer, dotnet::ObjectReference& out_object_reference);

    TypeID m_type_id;
    Name m_name;
    int m_static_index;
    uint32 m_num_descendants;
    Name m_parent_name;
    const HypClass* m_parent;
    HypClassAttributeSet m_attributes;
    EnumFlags<HypClassFlags> m_flags;

    SizeType m_size;
    SizeType m_alignment;

    Array<HypProperty*> m_properties;
    HashMap<Name, HypProperty*> m_properties_by_name;

    Array<HypMethod*> m_methods;
    HashMap<Name, HypMethod*> m_methods_by_name;

    Array<HypField*> m_fields;
    HashMap<Name, HypField*> m_fields_by_name;

    Array<HypConstant*> m_constants;
    HashMap<Name, HypConstant*> m_constants_by_name;

    EnumFlags<HypClassSerializationMode> m_serialization_mode;

private:
    mutable Weak<dotnet::Class> m_managed_class;
    mutable Mutex m_managed_class_mutex;
};

template <class T>
class HypClassInstance final : public HypClass
{
public:
    using PostLoadCallback = void (*)(T&);

    static HypClassInstance& GetInstance(
        Name name,
        int static_index,
        uint32 num_descendants,
        Name parent_name,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
    {
        static HypClassInstance instance { name, static_index, num_descendants, parent_name, attributes, flags, members };

        return instance;
    }

    HypClassInstance(
        Name name,
        int static_index,
        uint32 num_descendants,
        Name parent_name,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
        : HypClass(TypeID::ForType<T>(), name, static_index, num_descendants, parent_name, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~HypClassInstance() = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            return HypClassAllocationMethod::HANDLE;
        }
        else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            return HypClassAllocationMethod::REF_COUNTED_PTR;
        }
        else
        {
            return HypClassAllocationMethod::NONE;
        }
    }

    virtual bool GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const override
    {
        return GetManagedObjectFromObjectInitializer(HypClass::GetObjectInitializer(object_ptr), out_object_reference);
    }

    virtual bool CanCreateInstance() const override
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    virtual bool ToHypData(ByteView memory, HypData& out_hyp_data) const override
    {
        AssertThrowMsg(memory.Size() == sizeof(T),
            "Expected memory size to be %llu but got %llu! This could indicate a type safety violation.",
            sizeof(T), memory.Size());

        void* address = const_cast<void*>(reinterpret_cast<const void*>(memory.Data()));
        T* ptr = static_cast<T*>(address);

        if (UseHandles())
        {
            if constexpr (std::is_base_of_v<HypObjectBase, T>)
            {
                out_hyp_data = HypData(AnyHandle(ptr));
            }
            else
            {
                HYP_UNREACHABLE();
            }

            return true;
        }
        else if (UseRefCountedPtr())
        {
            if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
            {
                EnableRefCountedPtrFromThisBase<>* ptr_casted = static_cast<EnableRefCountedPtrFromThisBase<>*>(ptr);

                auto* ref_count_data = ptr_casted->weak_this.GetRefCountData_Internal();
                AssertDebug(ref_count_data != nullptr);

                RC<void> rc;
                rc.SetRefCountData_Internal(ptr_casted->weak_this.GetUnsafe(), ref_count_data, /* inc_ref */ true);

                out_hyp_data = HypData(std::move(rc));

                return true;
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }
        else
        {
            out_hyp_data = HypData(Any(ptr));

            return true;
        }

        return false;
    }

protected:
    virtual void FixupPointer(void* target, IHypObjectInitializer* new_initializer) const override
    {
        AssertThrow(target != nullptr);

        if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            // hack to update the TypeID of the weak_this pointer to be the same as the new initializer
            static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(target))->weak_this.GetRefCountData_Internal()->type_id = new_initializer->GetTypeID();
        }

        static_cast<T*>(target)->m_hyp_object_initializer_ptr = new_initializer;
    }

    virtual void PostLoad_Internal(void* object_ptr) const override
    {
        if (!object_ptr)
        {
            return;
        }

        const IHypClassCallbackWrapper* callback_wrapper = HypClassCallbackCollection<HypClassCallbackType::ON_POST_LOAD>::GetInstance().GetCallback(GetTypeID());

        if (!callback_wrapper)
        {
            return;
        }

        const HypClassCallbackWrapper<PostLoadCallback>* callback_wrapper_casted = dynamic_cast<const HypClassCallbackWrapper<PostLoadCallback>*>(callback_wrapper);
        AssertThrow(callback_wrapper_casted != nullptr);

        callback_wrapper_casted->GetCallback()(*static_cast<T*>(object_ptr));
    }

    virtual IHypObjectInitializer* GetObjectInitializer_Internal(void* object_ptr) const override
    {
        if (!object_ptr)
        {
            return nullptr;
        }

        return static_cast<T*>(object_ptr)->GetObjectInitializer();
    }

    virtual bool CreateInstance_Internal(HypData& out) const override
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            if constexpr (std::is_base_of_v<HypObjectBase, T>)
            {
                out = HypData(CreateObject<T>());

                return true;
            }
            else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
            {
                out = HypData(MakeRefCountedPtr<T>());

                return true;
            }
            else
            {
                return true;
            }
        }
        else
        {
            return false;
        }
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override
    {
        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            Array<Handle<T>> array;
            array.Reserve(elements.Size());

            for (SizeType i = 0; i < elements.Size(); i++)
            {
                if (!elements[i].Is<Handle<T>>())
                {
                    return false;
                }

                array.PushBack(std::move(elements[i].Get<Handle<T>>()));
            }

            out = HypData(std::move(array));

            return true;
        }
        else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            Array<RC<T>> array;
            array.Reserve(elements.Size());

            for (SizeType i = 0; i < elements.Size(); i++)
            {
                if (!elements[i].Is<RC<T>>())
                {
                    return false;
                }

                array.PushBack(std::move(elements[i].Get<RC<T>>()));
            }

            out = HypData(std::move(array));

            return true;
        }
        else
        {
            Array<T> array;
            array.Reserve(elements.Size());

            for (SizeType i = 0; i < elements.Size(); i++)
            {
                if (!elements[i].Is<T>())
                {
                    return false;
                }

                array.PushBack(std::move(elements[i].Get<T>()));
            }

            out = HypData(std::move(array));

            return true;
        }
    }

    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override
    {
        if constexpr (HYP_HAS_METHOD(T, GetHashCode))
        {
            return HashCode::GetHashCode(ref.Get<T>());
        }
        else
        {
            HYP_NOT_IMPLEMENTED();
        }
    }
};

class DynamicHypClassInstance final : public HypClass
{
public:
    DynamicHypClassInstance(TypeID type_id, Name name, const HypClass* parent_class, dotnet::Class* class_ptr, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members);
    virtual ~DynamicHypClassInstance() override;

    virtual bool IsValid() const override;

    virtual HypClassAllocationMethod GetAllocationMethod() const override;

    virtual bool GetManagedObject(const void* object_ptr, dotnet::ObjectReference& out_object_reference) const override;

    virtual bool CanCreateInstance() const override;

    virtual bool ToHypData(ByteView memory, HypData& out_hyp_data) const override;

protected:
    virtual void FixupPointer(void* target, IHypObjectInitializer* new_initializer) const override;
    virtual void PostLoad_Internal(void* object_ptr) const override;
    virtual IHypObjectInitializer* GetObjectInitializer_Internal(void* object_ptr) const override;
    virtual bool CreateInstance_Internal(HypData& out) const override;
    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override;
    virtual HashCode GetInstanceHashCode_Internal(ConstAnyRef ref) const override;
};

} // namespace hyperion

#endif