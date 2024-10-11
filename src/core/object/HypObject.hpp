/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_OBJECT_HPP
#define HYPERION_CORE_HYP_OBJECT_HPP

#include <core/Defines.hpp>
#include <core/Core.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/object/HypObjectEnums.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/TypeID.hpp>

#include <core/memory/UniquePtr.hpp>

#ifdef HYP_DEBUG_MODE
#include <core/threading/Threads.hpp>
#endif

namespace hyperion {

class HypClass;
struct HypData;

namespace dotnet {
class Object;
} // namespace dotnet

class IHypObjectInitializer;

extern HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *parent, TypeID type_id, const HypClass *hyp_class);
extern HYP_API const HypClass *GetClass(TypeID type_id);
extern HYP_API HypClassAllocationMethod GetHypClassAllocationMethod(const HypClass *hyp_class);

// Ensure the current HypObjectInitializer being constructed is the same as the one passed
extern HYP_API void CheckHypObjectInitializer(const IHypObjectInitializer *initializer, const void *address);
extern HYP_API void CleanupHypObjectInitializer(const HypClass *hyp_class, dotnet::Object *managed_object_ptr);

class IHypObjectInitializer
{
public:
    virtual ~IHypObjectInitializer() = default;

    virtual TypeID GetTypeID() const = 0;

    virtual const HypClass *GetClass() const = 0;

    virtual void SetManagedObject(dotnet::Object *managed_object) = 0;
    virtual dotnet::Object *GetManagedObject() const = 0;
};

template <class T>
class HypObjectInitializer final : public IHypObjectInitializer
{
public:
    HypObjectInitializer(void *native_address)
        : m_managed_object(nullptr)
    {
        CheckHypObjectInitializer(this, native_address);
        //InitHypObjectInitializer(this, native_address, T::GetTypeID(), T::GetClass());
    }

    virtual ~HypObjectInitializer() override
    {
#ifdef HYP_DEBUG_MODE
        HYP_MT_CHECK_RW(m_data_race_detector);
#endif

        CleanupHypObjectInitializer(GetClass_Static(), m_managed_object);
    }

    virtual TypeID GetTypeID() const override
    {
        return GetTypeID_Static();
    }

    virtual const HypClass *GetClass() const override
    {
        return GetClass_Static();
    }

    virtual void SetManagedObject(dotnet::Object *managed_object) override
    {
#ifdef HYP_DEBUG_MODE
        HYP_MT_CHECK_RW(m_data_race_detector);
#endif

        AssertThrow(m_managed_object == nullptr);

        m_managed_object = managed_object;
    }

    virtual dotnet::Object *GetManagedObject() const override
    {
#ifdef HYP_DEBUG_MODE
        HYP_MT_CHECK_READ(m_data_race_detector);
#endif

        return m_managed_object;
    }

    static TypeID GetTypeID_Static()
    {
        static constexpr TypeID type_id = TypeID::ForType<T>();

        return type_id;
    }

    static const HypClass *GetClass_Static()
    {
        static constexpr TypeID type_id = TypeID::ForType<T>();

        return ::hyperion::GetClass(type_id);
    }

private:
    dotnet::Object      *m_managed_object;

#ifdef HYP_DEBUG_MODE
    DataRaceDetector    m_data_race_detector;
#endif
};

// template <class T>
// struct HypObjectInitializerRef
// {
//     Variant<HypObjectInitializer<T>, IHypObjectInitializer *>   value;
//     IHypObjectInitializer                                       *ptr;

//     HypObjectInitializerRef()
//         : ptr(nullptr)
//     {
//     }

//     HypObjectInitializerRef
// };

#define HYP_OBJECT_BODY(T, ...) \
    private: \
        friend class HypObjectInitializer<T>; \
        \
        HypObjectInitializer<T> m_hyp_object_initializer { this }; \
        \
    public: \
        static constexpr bool is_hyp_object = true; \
        \
        HYP_FORCE_INLINE HypObjectInitializer<T> &GetObjectInitializer() \
            { return m_hyp_object_initializer; } \
        \
        HYP_FORCE_INLINE const HypObjectInitializer<T> &GetObjectInitializer() const \
            { return m_hyp_object_initializer; } \
        \
        HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const \
            { return m_hyp_object_initializer.GetManagedObject(); } \
        \
        static TypeID GetTypeID() \
        { \
            return HypObjectInitializer<T>::GetTypeID_Static(); \
        } \
        static const HypClass *GetClass() \
        { \
            return HypObjectInitializer<T>::GetClass_Static(); \
        } \
        \
    private:


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