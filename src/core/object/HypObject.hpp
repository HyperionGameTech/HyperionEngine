/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_OBJECT_HPP
#define HYPERION_CORE_HYP_OBJECT_HPP

#include <core/Defines.hpp>

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

class IHypObjectInitializer
{
public:
    virtual ~IHypObjectInitializer() = default;

    virtual void SetManagedObject(UniquePtr<dotnet::Object> &&managed_object) = 0;
};

template <class T>
class HypObjectInitializer final : public IHypObjectInitializer
{
public:
    HypObjectInitializer(T *parent)
    {
        InitHypObjectInitializer(this, parent, T::GetTypeID(), T::GetClass());
    }

    virtual ~HypObjectInitializer() override = default;

    virtual void SetManagedObject(UniquePtr<dotnet::Object> &&managed_object) override
    {
        m_managed_object = std::move(managed_object);
    }

    HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const
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

#define HYP_OBJECT_BODY(T) \
    private: \
        HypObjectInitializer<T> m_hyp_object_initializer { this }; \
        \
    public: \
        static constexpr bool is_hyp_object = true; \
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

} // namespace hyperion

#endif