/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/ObjectFwd.hpp>
#include <Core/reflection/ObjectEnums.hpp>
#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/Member.hpp>
#include <Core/reflection/ClassAttribute.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/containers/Array.hpp>

#include <Core/utilities/Span.hpp>

#include <Core/memory/UniquePtr.hpp>
#include <Core/memory/Any.hpp>
#include <Core/memory/AnyRef.hpp>

namespace Hyperion {

namespace dotnet {
class ManagedClass;
class ManagedObject;
struct ObjectReference;
} // namespace dotnet

class ObjectContainerBase;

struct MemberVariant;
class Property;
class Method;
class Field;
class StaticField;
class Class;
class Struct;

template <class T>
class ClassInstance;

template <class T>
class StructInstance;

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
enum EnginePoolName : int;
#endif

enum class ClassFlags : uint8
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

HYP_MAKE_ENUM_FLAGS(ClassFlags)

enum class ClassSerializationMode : uint8
{
    NONE = 0x0,

    MEMBERWISE = 0x1, // Use ObjectMarshal - Serialize members
    BITWISE = 0x2,    // Use ObjectMarshal - Serialize as FBOMStruct (binary)

    USE_MARSHAL_CLASS = 0x80, // Use Marshal class as override

    DEFAULT = MEMBERWISE | USE_MARSHAL_CLASS
};

HYP_MAKE_ENUM_FLAGS(ClassSerializationMode)

HYP_API extern const Class* g_hypObjectBaseClass;

#pragma region Helpers

HYP_API const Class* GetClass(const TypeId& typeId);
HYP_API const Class* GetClass(StringHash typeName);

HYP_API const Class* GetEnum(const TypeId& typeId);
HYP_API const Class* GetEnum(StringHash typeName);

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

HYP_API bool IsA(const Class* cls, const void* ptr, const TypeId& typeId);
HYP_API bool IsA(const Class* cls, const Class* instanceClass);

HYP_API const char* LookupTypeName(const TypeId& typeId);

#pragma endregion Helpers

class ClassMemberIterator
{
    enum class Phase
    {
        ITERATE_STATIC_FIELDS,
        ITERATE_PROPERTIES,
        ITERATE_METHODS,
        ITERATE_FIELDS,
        MAX
    };

    friend class ClassMemberList;

    static Phase NextPhase(EnumFlags<MemberType> allowedTypes, Phase current)
    {
        const auto getNext = [](Phase phase) -> Phase
        {
            if (phase == Phase::MAX)
            {
                return Phase::ITERATE_STATIC_FIELDS;
            }

            return static_cast<Phase>(static_cast<int>(phase) + 1);
        };

        const auto canDoNext = [allowedTypes](Phase nextPhase) -> bool
        {
            switch (nextPhase)
            {
            case Phase::ITERATE_STATIC_FIELDS:
                return allowedTypes & MemberType::StaticField;
            case Phase::ITERATE_PROPERTIES:
                return allowedTypes & MemberType::Property;
            case Phase::ITERATE_METHODS:
                return allowedTypes & MemberType::Method;
            case Phase::ITERATE_FIELDS:
                return allowedTypes & MemberType::Field;
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

    HYP_API ClassMemberIterator(const Class* cls, EnumFlags<MemberType> memberTypes, Phase phase, bool deep = true);

public:
    ClassMemberIterator(const ClassMemberIterator& other) = default;
    ClassMemberIterator& operator=(const ClassMemberIterator& other) = default;

    ClassMemberIterator(ClassMemberIterator&& other) noexcept = default;
    ClassMemberIterator& operator=(ClassMemberIterator&& other) noexcept = default;

    HYP_FORCE_INLINE bool operator==(const ClassMemberIterator& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const ClassMemberIterator& other) const = default;

    HYP_FORCE_INLINE ClassMemberIterator& operator++()
    {
        Advance();

        return *this;
    }

    ClassMemberIterator operator++(int)
    {
        ClassMemberIterator result(*this);
        ++result;
        return result;
    }

    HYP_FORCE_INLINE IMember& operator*()
    {
        return *m_currentValue;
    }

    HYP_FORCE_INLINE const IMember& operator*() const
    {
        return *m_currentValue;
    }

    HYP_FORCE_INLINE IMember* operator->()
    {
        return m_currentValue;
    }

    HYP_FORCE_INLINE const IMember* operator->() const
    {
        return m_currentValue;
    }

private:
    HYP_API void Advance();

    EnumFlags<MemberType> m_memberTypes;
    Phase m_phase;
    const Class* m_target;
    bool m_deep;

    SizeType m_currentIndex;
    mutable IMember* m_currentValue;
};

class ClassMemberList
{
public:
    using Iterator = ClassMemberIterator;
    using ConstIterator = ClassMemberIterator;

    ClassMemberList(const Class* cls, EnumFlags<MemberType> memberTypes, bool deep = true)
        : m_class(cls),
          m_memberTypes(memberTypes),
          m_deep(deep)
    {
    }

    ClassMemberList(const ClassMemberList& other) = default;
    ClassMemberList& operator=(const ClassMemberList& other) = default;

    ClassMemberList(ClassMemberList&& other) noexcept = default;
    ClassMemberList& operator=(ClassMemberList&& other) noexcept = default;

    ~ClassMemberList() = default;

    HYP_DEF_STL_BEGIN_END(ClassMemberIterator(m_class, m_memberTypes, ClassMemberIterator::Phase::ITERATE_PROPERTIES, m_deep), ClassMemberIterator(nullptr, m_memberTypes, ClassMemberIterator::Phase::MAX, m_deep))

private:
    const Class* m_class;
    EnumFlags<MemberType> m_memberTypes;
    bool m_deep;
};

enum class ClassCallbackType
{
    ON_POST_LOAD = 0
};

class IClassCallbackWrapper
{
public:
    virtual ~IClassCallbackWrapper() = default;
};

template <class Callback>
class ClassCallbackWrapper : public IClassCallbackWrapper
{
public:
    ClassCallbackWrapper(Callback&& callback)
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

template <ClassCallbackType CallbackType>
class ClassCallbackCollection
{
public:
    static ClassCallbackCollection& GetInstance()
    {
        static ClassCallbackCollection instance;
        return instance;
    }

    const IClassCallbackWrapper* GetCallback(const TypeId& typeId) const
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_callbacks.Find(typeId);

        if (it != m_callbacks.End())
        {
            return it->second;
        }

        return nullptr;
    }

    void SetCallback(const TypeId& typeId, const IClassCallbackWrapper* callback)
    {
        Mutex::Guard guard(m_mutex);

        m_callbacks[typeId] = callback;
    }

private:
    HashMap<TypeId, const IClassCallbackWrapper*> m_callbacks;
    mutable Mutex m_mutex;
};

template <ClassCallbackType CallbackType>
struct ClassCallbackRegistration
{
    template <auto Callback>
    ClassCallbackRegistration(const TypeId& typeId, ValueWrapper<Callback>)
    {
        static const ClassCallbackWrapper callbackWrapper(Callback);

        ClassCallbackCollection<CallbackType>::GetInstance().SetCallback(typeId, &callbackWrapper);
    }
};

class HYP_API Class
{
public:
    friend struct ClassRegistrationBase;
    friend class ObjectContainerBase;

    Class(
        TypeId typeId,
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members);

    Class(const Class& other) = delete;
    Class& operator=(const Class& other) = delete;
    Class(Class&& other) noexcept = delete;
    Class& operator=(Class&& other) noexcept = delete;
    virtual ~Class();

    virtual void Initialize();

    virtual bool IsValid() const
    {
        return false;
    }

    HYP_FORCE_INLINE ObjectContainerBase* GetObjectContainer() const
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

    virtual ClassAllocationMethod GetAllocationMethod() const = 0;

    HYP_FORCE_INLINE bool UseHandles() const
    {
        return GetAllocationMethod() == ClassAllocationMethod::HANDLE;
    }

    HYP_FORCE_INLINE bool IsReferenceCounted() const
    {
        return GetAllocationMethod() == ClassAllocationMethod::HANDLE;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    /*! \brief Returns the statically assigned index of this class in the global Class table.
     *  Only classes that are picked up by the build tool at configuration time will have a static index assigned.
     *  (Dynamic types created in managed code will return -1 for this)
     *  \note GetStaticIndex is used mainly for fast type checking internally, as it can be used to compare with Class::GetNumDescendants to check
     *  if the static index is within the expected range. It can also be used to preallocate slots for subclasses (see ResourceBinder in rendering/util/ResourceBinder.hpp for example usage) */
    HYP_FORCE_INLINE int GetStaticIndex() const
    {
        return m_staticIndex;
    }

    /*! \brief Returns the total number of descendants of this Class. */
    HYP_FORCE_INLINE uint32 GetNumDescendants() const
    {
        return m_numDescendants;
    }

    // Enum types only
    virtual TypeId GetUnderlyingTypeId() const
    {
        return TypeId::Void();
    }

    HYP_FORCE_INLINE const Class* GetParent() const
    {
        return m_parent;
    }

    /*! \brief Check if this Class is derived from the given Class or is equal to it.
     *  \param other The Class to check this against
     *  \return True if this Class is derived from or equal to the given Class, false otherwise. */
    bool IsDerivedFrom(const Class* other) const;

    /*! \brief Check if this Class is a base class of the given Class or is equal to it.
     *  \param other The Class to check against
     *  \return True if this Class is a base class of or equal to the given Class, false otherwise. */
    bool IsBaseOf(const Class* other) const;

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE const TypeInfo* GetTypeInfo() const
    {
        return m_typeInfo;
    }

    HYP_FORCE_INLINE EnumFlags<ClassFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE bool IsClassType() const
    {
        return m_flags & ClassFlags::CLASS_TYPE;
    }

    HYP_FORCE_INLINE bool IsStructType() const
    {
        return m_flags & ClassFlags::STRUCT_TYPE;
    }

    HYP_FORCE_INLINE bool IsEnumType() const
    {
        return m_flags & ClassFlags::ENUM_TYPE;
    }

    HYP_FORCE_INLINE bool IsPodType() const
    {
        return m_flags & ClassFlags::POD_TYPE;
    }

    HYP_FORCE_INLINE bool IsAbstract() const
    {
        return m_flags & ClassFlags::ABSTRACT;
    }

    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return m_flags & ClassFlags::DYNAMIC;
    }

    bool CanSerialize() const;

    HYP_FORCE_INLINE EnumFlags<ClassSerializationMode> GetSerializationMode() const
    {
        return m_serializationMode;
    }

    HYP_FORCE_INLINE const ClassAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE const ClassAttributeValue& GetAttribute(StringHash key) const
    {
        return m_attributes[key];
    }

    HYP_FORCE_INLINE const ClassAttributeValue& GetAttribute(StringHash key, const ClassAttributeValue& defaultValue) const
    {
        return m_attributes.Get(key, defaultValue);
    }

    HYP_FORCE_INLINE const ClassAttributeValue& GetAttributeDeep(StringHash key) const
    {
        static const ClassAttributeValue s_invalidValue {};

        return GetAttributeDeep(key, s_invalidValue);
    }

    const ClassAttributeValue& GetAttributeDeep(StringHash key, const ClassAttributeValue& defaultValue) const
    {
        const Class* cls = this;

        while (cls)
        {
            const ClassAttributeValue& value = cls->GetAttribute(key);

            if (value.IsValid())
            {
                return value;
            }

            cls = cls->GetParent();
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE ClassMemberList GetMembers(EnumFlags<MemberType> memberTypes, bool deep = true) const
    {
        return {
            this,
            memberTypes,
            deep
        };
    }

    HYP_FORCE_INLINE ClassMemberList GetMembers(bool includeProperties = true, bool deep = true) const
    {
        return {
            this,
            MemberType::Method
                | MemberType::Field
                | MemberType::StaticField
                | (includeProperties ? MemberType::Property : MemberType::None),
            deep
        };
    }

    IMember* GetMember(StringHash name, EnumFlags<MemberType> memberTypes = MemberType::All, bool deep = true) const;

    Property* GetProperty(StringHash name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<Property*>& GetProperties() const
    {
        return m_properties;
    }

    Array<Property*> GetPropertiesInherited() const;

    Method* GetMethod(StringHash name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<Method*>& GetMethods() const
    {
        return m_methods;
    }

    Array<Method*> GetMethodsInherited() const;

    Field* GetField(StringHash name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<Field*>& GetFields() const
    {
        return m_fields;
    }

    Array<Field*> GetFieldsInherited() const;

    StaticField* GetStaticField(StringHash name, bool deep = true) const;

    HYP_FORCE_INLINE const Array<StaticField*>& GetStaticFields() const
    {
        return m_staticFields;
    }

    Array<StaticField*> GetStaticFieldsInherited() const;

#ifdef HYP_DOTNET
    HYP_FORCE_INLINE RC<dotnet::ManagedClass> GetManagedClass() const
    {
        Mutex::Guard guard(m_managedClassMutex);

        return m_managedClass.Lock();
    }

    HYP_FORCE_INLINE void SetManagedClass(const RC<dotnet::ManagedClass>& managedClass) const
    {
        Mutex::Guard guard(m_managedClassMutex);

        m_managedClass = managedClass;
    }

    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const;
#endif

    virtual bool CanCreateInstance() const = 0;

    HYP_FORCE_INLINE bool CreateInstance(BoxedValue& out, bool allowAbstract = false) const
    {
        HYP_CORE_ASSERT(CanCreateInstance() && (allowAbstract || !IsAbstract()), "Cannot create a new instance for Class %s!\n\tCanCreateInstance: %s\tIsAbstract: %s\tAllow abstract: %s",
            GetName().LookupString(), CanCreateInstance() ? "true" : "false", IsAbstract() ? "true" : "false", allowAbstract ? "true" : "false");

        return CreateInstance_Internal(out);
    }

    HYP_FORCE_INLINE bool CreateInstanceArray(Span<BoxedValue> elements, BoxedValue& out, bool allowAbstract = false) const
    {
        HYP_CORE_ASSERT(CanCreateInstance() && (allowAbstract || !IsAbstract()), "Cannot create a new instance for Class %s!\n\tCanCreateInstance: %s\tIsAbstract: %s\tAllow abstract: %s",
            GetName().LookupString(), CanCreateInstance() ? "true" : "false", IsAbstract() ? "true" : "false", allowAbstract ? "true" : "false");

        return CreateInstanceArray_Internal(elements, out);
    }

    /*! \brief Create a new BoxedValue from \p memory. The object at \p memory must have the type of this Class's TypeId.
     *  The underlying data will be moved or have ownership taken.
     *  \param memory A view to the memory of the underlying object.
     *  \returns True if the operation was successful. */
    virtual HYP_DEPRECATED bool ToBoxed(ByteView memory, BoxedValue& outBoxed) const
    {
        return false;
    }

    void PostLoad(void* objectPtr) const
    {
        if (!objectPtr)
        {
            return;
        }

        const Class* cls = this;

        while (cls)
        {
            cls->PostLoad_Internal(objectPtr);

            cls = cls->GetParent();
        }
    }

protected:
    virtual void PostLoad_Internal(void* objectPtr) const
    {
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const
    {
        return false;
    }

    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const
    {
        return false;
    }

    void AddProperty(Property* property);
    void AddMethod(Method* method);
    void AddField(Field* field);
    void AddStaticField(StaticField* staticField);

    TypeId m_typeId;
    const TypeInfo* m_typeInfo;
    Name m_name;
    int m_staticIndex;
    uint32 m_numDescendants;
    Name m_parentName;
    const Class* m_parent;
    ClassAttributeSet m_attributes;
    EnumFlags<ClassFlags> m_flags;

    SizeType m_size;
    SizeType m_alignment;

    Array<Property*> m_properties;
    HashMap<Name, Property*> m_propertiesByName;

    Array<Method*> m_methods;
    HashMap<Name, Method*> m_methodsByName;

    Array<Field*> m_fields;
    HashMap<Name, Field*> m_fieldsByName;

    Array<StaticField*> m_staticFields;
    HashMap<Name, StaticField*> m_staticFieldsByName;

    EnumFlags<ClassSerializationMode> m_serializationMode;

    ObjectContainerBase* m_objectContainer;

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    EnginePoolName m_enginePoolName;
#endif

private:
    mutable Weak<dotnet::ManagedClass> m_managedClass;
    mutable Mutex m_managedClassMutex;
};

template <class T>
class ClassInstance final : public Class
{
public:
    using PostLoadCallback = void (*)(T&);

    static ClassInstance& GetInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members)
    {
        static ClassInstance s_instance { name, staticIndex, numDescendants, parentName, attributes, flags, members };

        return s_instance;
    }

    ClassInstance(
        Name name,
        int staticIndex,
        uint32 numDescendants,
        Name parentName,
        Span<const ClassAttribute> attributes,
        EnumFlags<ClassFlags> flags,
        Span<MemberVariant> members)
        : Class(TypeId::ForType<T>(), name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~ClassInstance() = default;

    virtual void Initialize() override
    {
        m_objectContainer = &GetObjectContainerMap().GetOrCreate<T>(this);
        m_objectContainer->Initialize();

        Class::Initialize();
    }

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual ClassAllocationMethod GetAllocationMethod() const override
    {
        if constexpr (std::is_base_of_v<ObjectBase, T>)
        {
            return ClassAllocationMethod::HANDLE;
        }
        else
        {
            return ClassAllocationMethod::NONE;
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

    virtual bool ToBoxed(ByteView memory, BoxedValue& outBoxed) const override
    {
        HYP_CORE_ASSERT(memory.Size() == sizeof(T),
            "Expected memory size to be %zu but got %zu! This could indicate a type safety violation.",
            sizeof(T), memory.Size());

        void* address = const_cast<void*>(reinterpret_cast<const void*>(memory.Data()));
        T* ptr = static_cast<T*>(address);

        if (UseHandles())
        {
            if constexpr (std::is_base_of_v<ObjectBase, T>)
            {
                outBoxed = BoxedValue(Handle<ObjectBase>::FromPointer(static_cast<ObjectBase*>(ptr)));
            }
            else
            {
                HYP_UNREACHABLE();
            }

            return true;
        }
        else
        {
            outBoxed = BoxedValue(Any(ptr));

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

        const IClassCallbackWrapper* callbackWrapper = ClassCallbackCollection<ClassCallbackType::ON_POST_LOAD>::GetInstance().GetCallback(GetTypeId());

        if (!callbackWrapper)
        {
            return;
        }

        const ClassCallbackWrapper<PostLoadCallback>* callbackWrapperCasted = dynamic_cast<const ClassCallbackWrapper<PostLoadCallback>*>(callbackWrapper);
        HYP_CORE_ASSERT(callbackWrapperCasted != nullptr);

        callbackWrapperCasted->GetCallback()(*static_cast<T*>(objectPtr));
    }

    virtual bool CreateInstance_Internal(BoxedValue& out) const override
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            if constexpr (std::is_base_of_v<ObjectBase, T>)
            {
                out = BoxedValue(MakeHandle<T>());

                return true;
            }
            else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
            {
                out = BoxedValue(MakeRefCountedPtr<T>());

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

    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override
    {
        if constexpr (std::is_base_of_v<ObjectBase, T>)
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

            out = BoxedValue(std::move(array));

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

            out = BoxedValue(std::move(array));

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

            out = BoxedValue(std::move(array));

            return true;
        }
    }

protected:
};

/*! \brief a runtime created Class instance, for use in scripts or external code such as .NET or HypScript */
class DynamicClassInstance final : public Class
{
public:
#ifdef HYP_DOTNET
    DynamicClassInstance(TypeId typeId, Name name, const Class* parentClass, dotnet::ManagedClass* pManagedClass, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<MemberVariant> members);
#endif

#ifdef HYP_SCRIPT
    DynamicClassInstance(TypeId typeId, Name name, const Class* parentClass, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<MemberVariant> members);
#endif

    virtual ~DynamicClassInstance() override;

    virtual bool IsValid() const override;

    virtual ClassAllocationMethod GetAllocationMethod() const override;

    virtual TypeId GetUnderlyingTypeId() const override;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override;
#endif

    virtual bool CanCreateInstance() const override;

    virtual bool ToBoxed(ByteView memory, BoxedValue& outBoxed) const override;

    using Class::AddField;
    using Class::AddMethod;
    using Class::AddProperty;
    using Class::AddStaticField;

    void SetField(uint32 index, Field* field);
    void SetMethod(uint32 index, Method* method);
    void SetProperty(uint32 index, Property* property);
    void SetConstant(uint32 index, StaticField* staticField);

    void AddRef();
    void Release();

protected:
    virtual void PostLoad_Internal(void* objectPtr) const override;
    virtual bool CreateInstance_Internal(BoxedValue& out) const override;
    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override;

    TypeId m_enumUnderlyingTypeId;

    volatile int32 m_refCount;
};

// Shared global attributes
namespace Attributes {

HYP_API extern const Name g_attrSerialize;
HYP_API extern const Name g_attrDeserialize;
HYP_API extern const Name g_attrTransient;
HYP_API extern const Name g_attrComponent;
HYP_API extern const Name g_attrSize;
HYP_API extern const Name g_attrNoScriptBindings;
HYP_API extern const Name g_attrOnlyLanguages;
HYP_API extern const Name g_attrCommand;
HYP_API extern const Name g_attrAbstract;
HYP_API extern const Name g_attrCompressed;
HYP_API extern const Name g_attrProperty;
HYP_API extern const Name g_attrLoadOrder;
HYP_API extern const Name g_attrJsonPath;
HYP_API extern const Name g_attrJsonIgnore;
HYP_API extern const Name g_attrScriptableDelegate;
HYP_API extern const Name g_attrFollowAssetPath;
HYP_API extern const Name g_attrSaveAsReference;

/// ===== Editor-specific attributes =====
HYP_API extern const Name g_attrEditor;        //!< legacy (deprecated) indicates that a property is editable in the editor
HYP_API extern const Name g_attrEditorOnly;    //!< this field/method/property is only available in editor builds (future proof - right now it has no effect)
HYP_API extern const Name g_attrEditOrder;     //!< order in which properties are displayed in the editor
HYP_API extern const Name g_attrEditEnabled;   //!< is editable in editor
HYP_API extern const Name g_attrEditHide;      //!< hide in editor
HYP_API extern const Name g_attrLabel;         //!< display name in editor (overrides property name)
HYP_API extern const Name g_attrDescription;   //!< help text for a property in inspector
HYP_API extern const Name g_attrEditAction;    //!< marks a method as an action button in the editor (e.g. "Bake Lighting")
HYP_API extern const Name g_attrEditCondition; //!< condition for editability or action availability in the editor (should be a method that returns bool)
/// ======================================

} // namespace Attributes

} // namespace Hyperion
