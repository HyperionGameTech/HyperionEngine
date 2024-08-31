/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_OBJECT_HPP
#define HYPERION_CORE_HYP_OBJECT_HPP

#include <core/Defines.hpp>

#include <core/object/HypObjectEnums.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/memory/UniquePtr.hpp>

namespace hyperion {

class HypClass;

namespace dotnet {
class Object;
} // namespace dotnet

class IHypObjectInitializer;

extern HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *parent, TypeID type_id, const HypClass *hyp_class);
extern HYP_API const HypClass *GetClass(TypeID type_id);

extern HYP_API HypClassAllocationMethod GetHypClassAllocationMethod(const HypClass *hyp_class);

class IHypObjectInitializer
{
public:
    virtual ~IHypObjectInitializer() = default;

    virtual void SetManagedObject(UniquePtr<dotnet::Object> &&managed_object) = 0;
    virtual dotnet::Object *GetManagedObject() const = 0;
};

template <class T>
class HypObjectInitializer final : public IHypObjectInitializer
{
public:
    HypObjectInitializer(void *native_address)
    {
        InitHypObjectInitializer(this, native_address, T::GetTypeID(), T::GetClass());
    }

    virtual ~HypObjectInitializer() override = default;

    virtual void SetManagedObject(UniquePtr<dotnet::Object> &&managed_object) override
    {
        m_managed_object = std::move(managed_object);
    }

    virtual dotnet::Object *GetManagedObject() const override
        { return m_managed_object.Get(); }

private:
    UniquePtr<dotnet::Object>   m_managed_object;
};

// #define HYP_OBJECT_BODY(T) \
//     private: \
//         HypObjectInitializer<T> m_hyp_object_initializer { this }; \
//         ID<T>                   m_id; \
//         \
//     public: \
//         static constexpr bool is_hyp_object = true; \
//         \
//         HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const \
//             { return m_hyp_object_initializer.GetManagedObject(); } \
//         \
//         static TypeID GetTypeID() \
//         { \
//             static constexpr TypeID type_id = TypeID::ForType<T>(); \
//             return type_id; \
//         } \
//         static const HypClass *GetClass() \
//         { \
//             static const HypClass *hyp_class = ::hyperion::GetClass(GetTypeID()); \
//             return hyp_class; \
//         } \
//         \
//         HYP_FORCE_INLINE ID<T> GetID() const \
//             { return m_id; } \
//         \
//         HYP_FORCE_INLINE void SetID(ID<T> id) \
//             { m_id = id; } \
//     private:

namespace detail {

template <class T, bool Condition = std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>>
struct HypObject_ToCommonBase_Impl;

template <class T>
struct HypObject_ToCommonBase_Impl<T, true>
{
    EnableRefCountedPtrFromThisBase<> *operator()(T *ptr) const
    {
        
        // Static cast needed because polymorphic types will have vtable inserted at start of
        // the object, making accessing `EnableRefCountedPtrFromThisBase<>::weak` invalid.
        return static_cast<EnableRefCountedPtrFromThisBase<> *>(ptr);
    }
};

template <class T>
struct HypObject_ToCommonBase_Impl<T, false>
{
    EnableRefCountedPtrFromThisBase<> *operator()(T *ptr) const
    {
        // Would fail at compile time on instantiation of HypClassInstance<T>, anyway - but throw an error to be sure

        HYP_FAIL("Class %s must inherit from EnableRefCountedPtrFromThis<T> for HypObject that does not use ObjectPool",
            TypeName<T>().Data());
        
        return nullptr;
    }
};

} // namespace detail

#define HYP_OBJECT_BODY(T, ...) \
    private: \
        friend class HypObjectInitializer<T>; \
        \
        void *GetHypObjectNativeAddress() \
        { \
            switch (GetHypClassAllocationMethod(GetClass())) { \
            case HypClassAllocationMethod::OBJECT_POOL_HANDLE: \
                return this; \
            case HypClassAllocationMethod::REF_COUNTED_PTR: \
                return detail::HypObject_ToCommonBase_Impl<T>{}(this); \
            default: \
                HYP_NOT_IMPLEMENTED(); \
            } \
        } \
        \
        HypObjectInitializer<T> m_hyp_object_initializer { /*GetHypObjectNativeAddress()*/ this }; \
        \
    public: \
        static constexpr bool is_hyp_object = true; \
        \
        HYP_FORCE_INLINE const HypObjectInitializer<T> &GetObjectInitializer() const \
            { return m_hyp_object_initializer; } \
        \
        HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const \
            { return m_hyp_object_initializer.GetManagedObject(); } \
        \
        static TypeID GetTypeID() \
        { \
            static constexpr TypeID type_id = TypeID::ForType<T>(); \
            return type_id; \
        } \
        static const HypClass *GetClass() \
        { \
            static const HypClass *hyp_class = ::hyperion::GetClass(GetTypeID()); \
            return hyp_class; \
        } \
    private:

template <class T, class T2 = void>
struct IsHypObject
{
    static constexpr bool value = false;
};

template <class T>
struct IsHypObject<T, std::enable_if_t< T::is_hyp_object > >
{
    static constexpr bool value = true;
};

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

} // namespace detail

template <class T>
using HypObjectType = typename detail::HypObjectType_Impl<T, std::bool_constant< IsHypObject<T>::value > >::Type;

// template <class T>
// HYP_FORCE_INLINE inline bool InitObject(HypObjectType<T> *object)
// {
//     static_assert(!has_opaque_handle_defined<T>, "Use InitObject(Handle<T>) overload instead");

//     if (!object) {
//         return false;
//     }

//     object

//     return true;
// }

} // namespace hyperion

#endif