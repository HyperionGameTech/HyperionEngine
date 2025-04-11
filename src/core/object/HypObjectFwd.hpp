/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_OBJECT_FWD_HPP
#define HYPERION_CORE_HYP_OBJECT_FWD_HPP

#include <core/Defines.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/ID.hpp>

#ifdef HYP_DEBUG_MODE
#include <core/threading/Threads.hpp>
#endif

#include <type_traits>

namespace hyperion {

namespace dotnet {
class Object;
class Class;
} // namespace dotnet

enum class HypClassAllocationMethod : uint8;

class HypClass;
class HypObjectBase;

template <class T>
class HypObject;

class IHypObjectInitializer;

struct HypObjectHeader;

template <class T>
struct HypObjectMemory;

class ManagedObjectResource;

enum class HypClassFlags : uint32;

extern HYP_API const HypClass *GetClass(TypeID type_id);

/*! \brief A base class for all HypObject classes that use memory pooling. */
class HYP_API HypObjectBase
{
public:
    template <class T>
    friend struct HypObjectMemory;

    virtual ~HypObjectBase() = default;

    TypeID GetTypeID() const;
    const HypClass *InstanceClass() const;

    HYP_FORCE_INLINE HypObjectHeader *GetObjectHeader_Internal() const
        { return m_header; }

protected:
    HYP_FORCE_INLINE IDBase GetID() const
        { return GetID_Internal(); }

private:
    IDBase GetID_Internal() const;

    // Pointer to the header of the object, holding container, index and ref counts
    HypObjectHeader *m_header;
};

class IHypObjectInitializer
{
public:
    virtual ~IHypObjectInitializer() = default;

    virtual TypeID GetTypeID() const = 0;

    virtual const HypClass *GetClass() const = 0;

    virtual dotnet::Class *GetManagedClass() const = 0;

    virtual void SetManagedObjectResource(ManagedObjectResource *managed_object_resource) = 0;
    virtual ManagedObjectResource *GetManagedObjectResource() const = 0;
    
    virtual dotnet::Object *GetManagedObject() const = 0;

    virtual void FixupPointer(void *_this, IHypObjectInitializer *ptr) = 0;

    virtual void IncRef(HypClassAllocationMethod allocation_method, void *_this, bool weak) const = 0;
    virtual void DecRef(HypClassAllocationMethod allocation_method, void *_this, bool weak) const = 0;
};

template <class T, class T2 = void>
struct IsHypObject
{
    static constexpr bool value = false;
};

/*! \brief A type trait to determine if a type is a HypObject (has a HypClass associated with it).
 *  \note A type is considered a HypObject if it is derived from HypObjectBase or if it has HYP_OBJECT_BODY(...) in the class body.
 *  \tparam T The type to check. */
template <class T>
struct IsHypObject<T, std::enable_if_t<std::is_base_of_v<HypObjectBase, T> || (T::HypObjectData::is_hyp_object /*&& std::is_same_v<T, typename T::HypObjectData::Type>*/)>>
{
    static constexpr bool value = true;

    using Type = std::conditional_t<std::is_base_of_v<HypObjectBase, T>, T, typename T::HypObjectData::Type>;
};

enum class HypObjectInitializerFlags : uint32
{
    NONE                                = 0x0,
    SUPPRESS_MANAGED_OBJECT_CREATION    = 0x1
};

HYP_MAKE_ENUM_FLAGS(HypObjectInitializerFlags)

extern HYP_API void PushHypObjectInitializerFlags(EnumFlags<HypObjectInitializerFlags> flags);
extern HYP_API void PopHypObjectInitializerFlags();

struct HypObjectInitializerFlagsGuard
{
    HypObjectInitializerFlagsGuard(EnumFlags<HypObjectInitializerFlags> flags)
    {
        PushHypObjectInitializerFlags(flags);
    }

    HypObjectInitializerFlagsGuard(const HypObjectInitializerFlagsGuard &other)                 = delete;
    HypObjectInitializerFlagsGuard &operator=(const HypObjectInitializerFlagsGuard &other)      = delete;
    HypObjectInitializerFlagsGuard(HypObjectInitializerFlagsGuard &&other) noexcept             = delete;
    HypObjectInitializerFlagsGuard &operator=(HypObjectInitializerFlagsGuard &&other) noexcept  = delete;

    ~HypObjectInitializerFlagsGuard()
    {
        PopHypObjectInitializerFlags();
    }
};

struct HypObjectInitializerGuardBase
{
    HYP_API HypObjectInitializerGuardBase(const HypClass *hyp_class, void *address);
    HYP_API ~HypObjectInitializerGuardBase();

    const HypClass  *hyp_class;
    void            *address;

#ifdef HYP_DEBUG_MODE
    ThreadID        initializer_thread_id;
#else
    uint32          count;
#endif
};

template <class T>
struct HypObjectInitializerGuard : HypObjectInitializerGuardBase
{
    HypObjectInitializerGuard(void *address)
        : HypObjectInitializerGuardBase(GetClassAndEnsureValid(), address)
    {
    }

    static const HypClass *GetClassAndEnsureValid()
    {
        using HypObjectType = typename IsHypObject<T>::Type;
        static const HypClass *hyp_class = GetClass(TypeID::ForType<HypObjectType>());

        AssertThrowMsg(hyp_class != nullptr, "HypClass not registered for type %s", TypeNameWithoutNamespace<HypObjectType>().Data());

        return hyp_class;
    }
};

class HypObjectPtr
{
public:
    HypObjectPtr()
        : m_ptr(nullptr),
          m_hyp_class(nullptr)
    {
    }

    HypObjectPtr(std::nullptr_t)
        : m_ptr(nullptr),
          m_hyp_class(nullptr)
    {
    }

    HypObjectPtr(const HypClass *hyp_class, void *ptr)
        : m_ptr(ptr),
          m_hyp_class(hyp_class)
    {
    }

    template <class T, typename = std::enable_if_t< IsHypObject<T>::value > >
    HypObjectPtr(T *ptr)
        : m_ptr(ptr),
          m_hyp_class(GetHypClass(TypeID::ForType<typename IsHypObject<T>::Type>()))
    {
    }

    HypObjectPtr(const HypObjectPtr &other)                 = default;
    HypObjectPtr &operator=(const HypObjectPtr &other)      = default;
    HypObjectPtr(HypObjectPtr &&other) noexcept             = default;
    HypObjectPtr &operator=(HypObjectPtr &&other) noexcept  = default;
    ~HypObjectPtr()                                         = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return m_ptr != nullptr && m_hyp_class != nullptr; }

    HYP_FORCE_INLINE bool operator!() const
        { return !m_ptr || !m_hyp_class; }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return m_ptr == nullptr; }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return m_ptr != nullptr; }

    HYP_FORCE_INLINE bool operator==(const HypObjectPtr &other) const
        { return m_ptr == other.m_ptr && m_hyp_class == other.m_hyp_class; }

    HYP_FORCE_INLINE bool operator!=(const HypObjectPtr &other) const
        { return m_ptr != other.m_ptr || m_hyp_class != other.m_hyp_class; }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_ptr != nullptr && m_hyp_class != nullptr; }

    HYP_FORCE_INLINE const HypClass *GetClass() const
        { return m_hyp_class; }

    HYP_FORCE_INLINE void *GetPointer() const
        { return m_ptr; }

    HYP_API IHypObjectInitializer *GetObjectInitializer() const;

    HYP_API uint32 GetRefCount_Strong() const;

    HYP_API void IncRef(bool weak = false);
    HYP_API void DecRef(bool weak = false);

private:
    HYP_API const HypClass *GetHypClass(TypeID type_id) const;

    void            *m_ptr;
    const HypClass  *m_hyp_class;
};

HYP_API void HypObject_OnIncRefCount_Strong(HypObjectPtr ptr, uint32 count);
HYP_API void HypObject_OnDecRefCount_Strong(HypObjectPtr ptr, uint32 count);
HYP_API void HypObject_OnIncRefCount_Weak(HypObjectPtr ptr, uint32 count);
HYP_API void HypObject_OnDecRefCount_Weak(HypObjectPtr ptr, uint32 count);

namespace detail {

template <class T, class T2>
struct HypObjectType_Impl;

template <class T>
struct HypObjectType_Impl<T, std::false_type>;

template <class T>
struct HypObjectType_Impl<T, std::true_type>
{
    using Type = T;
};

template <class T>
struct HypClassRegistration;

} // namespace detail

template <class T>
using HypObjectType = typename detail::HypObjectType_Impl<T, std::bool_constant< IsHypObject<T>::value > >::Type;

} // namespace hyperion

#endif