/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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
class HypObjectContainerBase;

struct HypMember;
class HypProperty;
class HypMethod;
class HypField;
class HypConstant;
class HypClass;
class HypStruct;

template <class T>
class HypClassInstance;

template <class T>
class HypStructInstance;

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
enum EnginePoolName : int;
#endif

HYP_ENUM()
enum class HypClassFlags : uint8
{
    NONE = 0x0,
    CLASS_TYPE = 0x1,
    STRUCT_TYPE = 0x2,
    ENUM_TYPE = 0x4,
    ABSTRACT = 0x8,
    POD_TYPE = 0x10,
    DYNAMIC = 0x20, // Dynamic classes are not registered in the class registry
    NO_SCRIPT_BINDINGS = 0x40,
    ANONYMOUS = 0x80 // not registered in the class registry
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

HYP_API extern const HypClass* g_hypObjectBaseClass;

#pragma region Helpers

HYP_API const HypClass* GetClass(TypeId typeId);
HYP_API const HypClass* GetClass(WeakName typeName);
HYP_API const HypClass* GetEnum(TypeId typeId);
HYP_API const HypClass* GetEnum(WeakName typeName);

HYP_API SizeType GetNumDescendants(TypeId typeId);

template <class T>
static inline SizeType GetNumDescendants()
{
    return GetNumDescendants(TypeId::ForType<T>());
}

HYP_API int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

template <class T, class U>
static inline int GetSubclassIndex()
{
    static const int s_subclassIndex = GetSubclassIndex(TypeId::ForType<T>(), TypeId::ForType<U>());

    return s_subclassIndex;
}

template <class T>
HYP_FORCE_INLINE const HypClass* GetEnum()
{
    return GetEnum(TypeId::ForType<T>());
}

HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);
HYP_API bool IsA(const HypClass* hypClass, const HypClass* instanceHypClass);

HYP_API const char* LookupTypeName(TypeId typeId);

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

    static Phase NextPhase(EnumFlags<HypMemberType> allowedTypes, Phase current)
    {
        const auto getNext = [](Phase phase) -> Phase
        {
            if (phase == Phase::MAX)
            {
                return Phase::ITERATE_CONSTANTS;
            }

            return static_cast<Phase>(static_cast<int>(phase) + 1);
        };

        const auto canDoNext = [allowedTypes](Phase nextPhase) -> bool
        {
            switch (nextPhase)
            {
            case Phase::ITERATE_CONSTANTS:
                return allowedTypes & HypMemberType::TYPE_CONSTANT;
            case Phase::ITERATE_PROPERTIES:
                return allowedTypes & HypMemberType::TYPE_PROPERTY;
            case Phase::ITERATE_METHODS:
                return allowedTypes & HypMemberType::TYPE_METHOD;
            case Phase::ITERATE_FIELDS:
                return allowedTypes & HypMemberType::TYPE_FIELD;
            default:
                return true;
            }
        };

        Phase nextPhase = getNext(current);

        while (!canDoNext(nextPhase))
        {
            nextPhase = getNext(nextPhase);
        }

        return nextPhase;
    }

    HYP_API HypClassMemberIterator(const HypClass* hypClass, EnumFlags<HypMemberType> memberTypes, Phase phase, bool deep = true);

public:
    HypClassMemberIterator(const HypClassMemberIterator& other) = default;
    HypClassMemberIterator& operator=(const HypClassMemberIterator& other) = default;

    HypClassMemberIterator(HypClassMemberIterator&& other) noexcept = default;
    HypClassMemberIterator& operator=(HypClassMemberIterator&& other) noexcept = default;

    HYP_FORCE_INLINE bool operator==(const HypClassMemberIterator& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const HypClassMemberIterator& other) const = default;

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
        return *m_currentValue;
    }

    HYP_FORCE_INLINE const IHypMember& operator*() const
    {
        return *m_currentValue;
    }

    HYP_FORCE_INLINE IHypMember* operator->()
    {
        return m_currentValue;
    }

    HYP_FORCE_INLINE const IHypMember* operator->() const
    {
        return m_currentValue;
    }

private:
    HYP_API void Advance();

    EnumFlags<HypMemberType> m_memberTypes;
    Phase m_phase;
    const HypClass* m_target;
    bool m_deep;

    SizeType m_currentIndex;
    mutable IHypMember* m_currentValue;
};

class HypClassMemberList
{
public:
    using Iterator = HypClassMemberIterator;
    using ConstIterator = HypClassMemberIterator;

    HypClassMemberList(const HypClass* hypClass, EnumFlags<HypMemberType> memberTypes, bool deep = true)
        : m_hypClass(hypClass),
          m_memberTypes(memberTypes),
          m_deep(deep)
    {
    }

    HypClassMemberList(const HypClassMemberList& other) = default;
    HypClassMemberList& operator=(const HypClassMemberList& other) = default;

    HypClassMemberList(HypClassMemberList&& other) noexcept = default;
    HypClassMemberList& operator=(HypClassMemberList&& other) noexcept = default;

    ~HypClassMemberList() = default;

    HYP_DEF_STL_BEGIN_END(HypClassMemberIterator(m_hypClass, m_memberTypes, HypClassMemberIterator::Phase::ITERATE_PROPERTIES, m_deep), HypClassMemberIterator(nullptr, m_memberTypes, HypClassMemberIterator::Phase::MAX, m_deep))

private:
    const HypClass* m_hypClass;
    EnumFlags<HypMemberType> m_memberTypes;
    bool m_deep;
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

    const IHypClassCallbackWrapper* GetCallback(const TypeId& typeId) const
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_callbacks.Find(typeId);

        if (it != m_callbacks.End())
        {
            return it->second;
        }

        return nullptr;
    }

    void SetCallback(const TypeId& typeId, const IHypClassCallbackWrapper* callback)
    {
        Mutex::Guard guard(m_mutex);

        m_callbacks[typeId] = callback;
    }

private:
    HashMap<TypeId, const IHypClassCallbackWrapper*> m_callbacks;
    mutable Mutex m_mutex;
};

template <HypClassCallbackType CallbackType>
struct HypClassCallbackRegistration
{
    template <auto Callback>
    HypClassCallbackRegistration(const TypeId& typeId, ValueWrapper<Callback>)
    {
        static const HypClassCallbackWrapper callbackWrapper(Callback);

        HypClassCallbackCollection<CallbackType>::GetInstance().SetCallback(typeId, &callbackWrapper);
    }
};

class HYP_API HypClass
{
public:
    friend struct HypClassRegistrationBase;
    friend class HypObjectPool;
    friend class HypObjectContainerBase;

    HypClass(
        TypeId typeId,
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members);

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

    HYP_FORCE_INLINE HypObjectContainerBase* GetObjectContainer() const
    {
        return m_objectContainer;
    }

    HYP_FORCE_INLINE SizeType GetSize() const
    {
        return m_size;
    }

    HYP_FORCE_INLINE SizeType GetAlignment() const
    {
        return m_alignment;
    }

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    HYP_FORCE_INLINE EnginePoolName GetEnginePoolName() const
    {
        return m_enginePoolName;
    }
#endif

    virtual HypClassAllocationMethod GetAllocationMethod() const = 0;

    HYP_FORCE_INLINE bool UseHandles() const
    {
        return GetAllocationMethod() == HypClassAllocationMethod::HANDLE;
    }

    HYP_FORCE_INLINE bool IsReferenceCounted() const
    {
        return GetAllocationMethod() == HypClassAllocationMethod::HANDLE;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    /*! \brief Returns the statically assigned index of this class in the global HypClass table.
     *  Only classes that are picked up by the build tool at configuration time will have a static index assigned.
     *  (Dynamic types created in managed code will return -1 for this)
     *  \note GetStaticIndex is used mainly for fast type checking internally, as it can be used to compare with HypClass::GetNumDescendants to check
     *  if the static index is within the expected range. It can also be used to preallocate slots for subclasses (see ResourceBinder in rendering/util/ResourceBinder.hpp for example usage) */
    HYP_FORCE_INLINE int GetStaticIndex() const
    {
        return m_staticIndex;
    }

    /*! \brief Returns the total number of descendants of this HypClass. */
    HYP_FORCE_INLINE uint32 GetNumDescendants() const
    {
        return m_numDescendants;
    }

    // Enum types only
    virtual TypeId GetUnderlyingTypeId() const
    {
        return TypeId::Void();
    }

    HYP_FORCE_INLINE const HypClass* GetParent() const
    {
        return m_parent;
    }

    /*! \brief Check if this HypClass is derived from the given HypClass or is equal to it.
     *  \param other The HypClass to check this against
     *  \return True if this HypClass is derived from or equal to the given HypClass, false otherwise. */
    bool IsDerivedFrom(const HypClass* other) const;

    /*! \brief Check if this HypClass is a base class of the given HypClass or is equal to it.
     *  \param other The HypClass to check against
     *  \return True if this HypClass is a base class of or equal to the given HypClass, false otherwise. */
    bool IsBaseOf(const HypClass* other) const;

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE const TypeInfo* GetTypeInfo() const
    {
        return m_typeInfo;
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

    HYP_FORCE_INLINE bool IsPodType() const
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
        return m_serializationMode;
    }

    HYP_FORCE_INLINE const HypClassAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE const HypClassAttributeValue& GetAttribute(WeakName key) const
    {
        return m_attributes[key];
    }

    HYP_FORCE_INLINE const HypClassAttributeValue& GetAttribute(WeakName key, const HypClassAttributeValue& defaultValue) const
    {
        return m_attributes.Get(key, defaultValue);
    }

    HYP_FORCE_INLINE const HypClassAttributeValue& GetAttributeDeep(WeakName key) const
    {
        static const HypClassAttributeValue s_invalidValue {};

        return GetAttributeDeep(key, s_invalidValue);
    }

    const HypClassAttributeValue& GetAttributeDeep(WeakName key, const HypClassAttributeValue& defaultValue) const
    {
        const HypClass* hypClass = this;

        while (hypClass)
        {
            const HypClassAttributeValue& value = hypClass->GetAttribute(key);

            if (value.IsValid())
            {
                return value;
            }

            hypClass = hypClass->GetParent();
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE HypClassMemberList GetMembers(EnumFlags<HypMemberType> memberTypes, bool deep = true) const
    {
        return {
            this,
            memberTypes,
            deep
        };
    }

    HYP_FORCE_INLINE HypClassMemberList GetMembers(bool includeProperties = true, bool deep = true) const
    {
        return {
            this,
            HypMemberType::TYPE_METHOD
                | HypMemberType::TYPE_FIELD
                | HypMemberType::TYPE_CONSTANT
                | (includeProperties ? HypMemberType::TYPE_PROPERTY : HypMemberType::NONE),
            deep
        };
    }

    IHypMember* GetMember(WeakName name, EnumFlags<HypMemberType> memberTypes = HypMemberType::ALL, bool deep = true) const;

    HypProperty* GetProperty(WeakName name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<HypProperty*>& GetProperties() const
    {
        return m_properties;
    }

    Array<HypProperty*> GetPropertiesInherited() const;

    HypMethod* GetMethod(WeakName name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<HypMethod*>& GetMethods() const
    {
        return m_methods;
    }

    Array<HypMethod*> GetMethodsInherited() const;

    HypField* GetField(WeakName name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<HypField*>& GetFields() const
    {
        return m_fields;
    }

    Array<HypField*> GetFieldsInherited() const;

    HypConstant* GetConstant(WeakName name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<HypConstant*>& GetConstants() const
    {
        return m_constants;
    }

    Array<HypConstant*> GetConstantsInherited() const;

#ifdef HYP_DOTNET
    HYP_FORCE_INLINE RC<dotnet::Class> GetManagedClass() const
    {
        Mutex::Guard guard(m_managedClassMutex);

        return m_managedClass.Lock();
    }

    HYP_FORCE_INLINE void SetManagedClass(const RC<dotnet::Class>& managedClass) const
    {
        Mutex::Guard guard(m_managedClassMutex);

        m_managedClass = managedClass;
    }

    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const;
#endif

    virtual bool CanCreateInstance() const = 0;

    HYP_FORCE_INLINE bool CreateInstance(HypData& out, bool allowAbstract = false) const
    {
        HYP_CORE_ASSERT(CanCreateInstance() && (allowAbstract || !IsAbstract()), "Cannot create a new instance for HypClass %s!\n\tCanCreateInstance: %s\tIsAbstract: %s\tAllow abstract: %s",
            GetName().LookupString(), CanCreateInstance() ? "true" : "false", IsAbstract() ? "true" : "false", allowAbstract ? "true" : "false");

        return CreateInstance_Internal(out);
    }

    HYP_FORCE_INLINE bool CreateInstanceArray(Span<HypData> elements, HypData& out, bool allowAbstract = false) const
    {
        HYP_CORE_ASSERT(CanCreateInstance() && (allowAbstract || !IsAbstract()), "Cannot create a new instance for HypClass %s!\n\tCanCreateInstance: %s\tIsAbstract: %s\tAllow abstract: %s",
            GetName().LookupString(), CanCreateInstance() ? "true" : "false", IsAbstract() ? "true" : "false", allowAbstract ? "true" : "false");

        return CreateInstanceArray_Internal(elements, out);
    }

    /*! \brief Create a new HypData from \ref{memory}. The object at \ref{memory} must have the type of this HypClass's TypeId.
     *  The underlying data will be moved or have ownership taken.
     *  \param memory A view to the memory of the underlying object.
     *  \returns True if the operation was successful. */
    virtual bool ToHypData(ByteView memory, HypData& outHypData) const
    {
        return false;
    }

    void PostLoad(void* objectPtr) const
    {
        if (!objectPtr)
        {
            return;
        }

        const HypClass* hypClass = this;

        while (hypClass)
        {
            hypClass->PostLoad_Internal(objectPtr);

            hypClass = hypClass->GetParent();
        }
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const
    {
    }

    virtual bool CreateInstance_Internal(HypData& out) const
    {
        return false;
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const
    {
        return false;
    }

    void AddProperty(HypProperty* property);
    void AddMethod(HypMethod* method);
    void AddField(HypField* field);
    void AddConstant(HypConstant* constant);

    TypeId m_typeId;
    const TypeInfo* m_typeInfo;
    Name m_name;
    int m_staticIndex;
    uint32 m_numDescendants;
    Name m_parentName;
    const HypClass* m_parent;
    HypClassAttributeSet m_attributes;
    EnumFlags<HypClassFlags> m_flags;

    SizeType m_size;
    SizeType m_alignment;

    Array<HypProperty*> m_properties;
    HashMap<Name, HypProperty*> m_propertiesByName;

    Array<HypMethod*> m_methods;
    HashMap<Name, HypMethod*> m_methodsByName;

    Array<HypField*> m_fields;
    HashMap<Name, HypField*> m_fieldsByName;

    Array<HypConstant*> m_constants;
    HashMap<Name, HypConstant*> m_constantsByName;

    EnumFlags<HypClassSerializationMode> m_serializationMode;

    HypObjectContainerBase* m_objectContainer;

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    EnginePoolName m_enginePoolName;
#endif

private:
    mutable Weak<dotnet::Class> m_managedClass;
    mutable Mutex m_managedClassMutex;
};

template <class T>
class HypClassInstance final : public HypClass
{
public:
    using PostLoadCallback = void (*)(T&);

    static HypClassInstance& GetInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
    {
        static HypClassInstance s_instance { name, staticIndex, numDescendants, parentName, attributes, flags, members };

        return s_instance;
    }

    HypClassInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const HypClassAttribute> attributes,
        EnumFlags<HypClassFlags> flags,
        Span<HypMember> members)
        : HypClass(TypeId::ForType<T>(), name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);

        m_objectContainer = &HypObjectPool::GetObjectContainerMap().GetOrCreate<T>(this);
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
        else
        {
            return HypClassAllocationMethod::NONE;
        }
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

    virtual bool ToHypData(ByteView memory, HypData& outHypData) const override
    {
        HYP_CORE_ASSERT(memory.Size() == sizeof(T),
            "Expected memory size to be %zu but got %zu! This could indicate a type safety violation.",
            sizeof(T), memory.Size());

        void* address = const_cast<void*>(reinterpret_cast<const void*>(memory.Data()));
        T* ptr = static_cast<T*>(address);

        if (UseHandles())
        {
            if constexpr (std::is_base_of_v<HypObjectBase, T>)
            {
                outHypData = HypData(AnyHandle(ptr));
            }
            else
            {
                HYP_UNREACHABLE();
            }

            return true;
        }
        else
        {
            outHypData = HypData(Any(ptr));

            return true;
        }

        return false;
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override
    {
        if (!objectPtr)
        {
            return;
        }

        const IHypClassCallbackWrapper* callbackWrapper = HypClassCallbackCollection<HypClassCallbackType::ON_POST_LOAD>::GetInstance().GetCallback(GetTypeId());

        if (!callbackWrapper)
        {
            return;
        }

        const HypClassCallbackWrapper<PostLoadCallback>* callbackWrapperCasted = dynamic_cast<const HypClassCallbackWrapper<PostLoadCallback>*>(callbackWrapper);
        HYP_CORE_ASSERT(callbackWrapperCasted != nullptr);

        callbackWrapperCasted->GetCallback()(*static_cast<T*>(objectPtr));
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

protected:
};

/*! \brief a runtime created HypClass instance, for use in scripts or external code such as .NET or HypScript */
class DynamicHypClassInstance final : public HypClass
{
public:
#ifdef HYP_DOTNET
    DynamicHypClassInstance(TypeId typeId, Name name, const HypClass* parentClass, dotnet::Class* classPtr, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members);
#endif

#ifdef HYP_SCRIPT
    DynamicHypClassInstance(TypeId typeId, Name name, const HypClass* parentClass, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members);
#endif

    virtual ~DynamicHypClassInstance() override;

    virtual bool IsValid() const override;

    virtual HypClassAllocationMethod GetAllocationMethod() const override;

    virtual TypeId GetUnderlyingTypeId() const override;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override;
#endif

    virtual bool CanCreateInstance() const override;

    virtual bool ToHypData(ByteView memory, HypData& outHypData) const override;

    using HypClass::AddConstant;
    using HypClass::AddField;
    using HypClass::AddMethod;
    using HypClass::AddProperty;

    void SetField(uint32 index, HypField* field);
    void SetMethod(uint32 index, HypMethod* method);
    void SetProperty(uint32 index, HypProperty* property);
    void SetConstant(uint32 index, HypConstant* constant);

    void AddRef();
    void Release();

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override;
    virtual bool CreateInstance_Internal(HypData& out) const override;
    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override;

    TypeId m_enumUnderlyingTypeId;

    volatile int32 m_refCount;
};

// Shared global attributes
namespace Attributes {

HYP_API extern const Name g_attrSerialize;
HYP_API extern const Name g_attrTransient;
HYP_API extern const Name g_attrComponent;
HYP_API extern const Name g_attrSize;
HYP_API extern const Name g_attrNoScriptBindings;
HYP_API extern const Name g_attrCommand;
HYP_API extern const Name g_attrAbstract;
HYP_API extern const Name g_attrDescription;
HYP_API extern const Name g_attrCompressed;
HYP_API extern const Name g_attrEditor;
HYP_API extern const Name g_attrProperty;
HYP_API extern const Name g_attrLoadOrder;
HYP_API extern const Name g_attrJsonPath;
HYP_API extern const Name g_attrJsonIgnore;

} // namespace Attributes

} // namespace hyperion
