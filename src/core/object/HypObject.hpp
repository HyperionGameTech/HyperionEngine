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
class Class;
} // namespace dotnet

extern HYP_API void InitHypObjectInitializer(IHypObjectInitializer *initializer, void *parent, TypeID type_id, const HypClass *hyp_class, UniquePtr<dotnet::Object> &&managed_object);
extern HYP_API const HypClass *GetClass(TypeID type_id);
extern HYP_API HypClassAllocationMethod GetHypClassAllocationMethod(const HypClass *hyp_class);
extern HYP_API dotnet::Class *GetHypClassManagedClass(const HypClass *hyp_class);

// Ensure the current HypObjectInitializer being constructed is the same as the one passed
extern HYP_API void CheckHypObjectInitializer(const IHypObjectInitializer *initializer, TypeID type_id, const HypClass *hyp_class, const void *address);
extern HYP_API void CleanupHypObjectInitializer(const HypClass *hyp_class, dotnet::Object *managed_object_ptr);

class IHypObjectInitializer
{
public:
    virtual ~IHypObjectInitializer() = default;

    virtual TypeID GetTypeID() const = 0;

    virtual const HypClass *GetClass() const = 0;

    virtual dotnet::Class *GetManagedClass() const = 0;

    virtual void SetManagedObject(dotnet::Object *managed_object) = 0;
    virtual dotnet::Object *GetManagedObject() const = 0;

    virtual void FixupPointer(void *_this, IHypObjectInitializer *ptr) = 0;
};

template <class T>
class HypObjectInitializer final : public IHypObjectInitializer
{
    // static const void (*s_fixup_fptr)(void *_this, IHypObjectInitializer *ptr);

public:
    HypObjectInitializer(T *_this)
        : m_managed_object(nullptr)
    {
        CheckHypObjectInitializer(this, GetTypeID_Static(), GetClass_Static(), _this);
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

    virtual dotnet::Class *GetManagedClass() const override
    {
        return GetHypClassManagedClass(GetClass_Static());
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

    virtual void FixupPointer(void *_this, IHypObjectInitializer *ptr) override
    {
        static_cast<T *>(_this)->m_hyp_object_initializer_ptr = ptr;
    }

    HYP_FORCE_INLINE static TypeID GetTypeID_Static()
    {
        static constexpr TypeID type_id = TypeID::ForType<T>();

        return type_id;
    }

    HYP_FORCE_INLINE static const HypClass *GetClass_Static()
    {
        static const HypClass *hyp_class = ::hyperion::GetClass(TypeID::ForType<T>());

        return hyp_class;
    }

private:
    dotnet::Object      *m_managed_object;

#ifdef HYP_DEBUG_MODE
    DataRaceDetector    m_data_race_detector;
#endif
};

// template <class T>
// const void (*HypObjectInitializer<T>::s_fixup_fptr)(void *, IHypObjectInitializer *) = [](void *_this, IHypObjectInitializer *_ptr)
// {
// };

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
        IHypObjectInitializer   *m_hyp_object_initializer_ptr = &m_hyp_object_initializer; \
        \
    public: \
        struct HypObjectData \
        { \
            using Type = T; \
            \
            static constexpr bool is_hyp_object = true; \
        }; \
        \
        HYP_FORCE_INLINE IHypObjectInitializer *GetObjectInitializer() const \
            { return m_hyp_object_initializer_ptr; } \
        \
        HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const \
            { return m_hyp_object_initializer_ptr->GetManagedObject(); } \
        \
        HYP_FORCE_INLINE const HypClass *InstanceClass() const \
            { return m_hyp_object_initializer_ptr->GetClass(); } \
        \
        HYP_FORCE_INLINE static const HypClass *Class() \
            { return HypObjectInitializer<T>::GetClass_Static(); } \
        \
        template <class TOther> \
        HYP_FORCE_INLINE bool IsInstanceOf() const \
        { \
            if constexpr (std::is_same_v<T, TOther> || std::is_base_of_v<TOther, T>) { \
                return true; \
            } else { \
                const HypClass *other_hyp_class = GetClass<TOther>(); \
                if (!other_hyp_class) { \
                    return false; \
                } \
                return IsInstanceOfHypClass(other_hyp_class, InstanceClass()); \
            } \
        } \
        \
    private:

class HypObjectPtr
{
public:
    template <class T, typename = std::enable_if_t< IsHypObject<T>::value > >
    HypObjectPtr(T *ptr)
        : m_ptr(ptr),
          m_hyp_class(GetHypClass(TypeID::ForType<T>()))
    {
    }

    HypObjectPtr(const HypObjectPtr &other)                 = default;
    HypObjectPtr &operator=(const HypObjectPtr &other)      = default;
    HypObjectPtr(HypObjectPtr &&other) noexcept             = default;
    HypObjectPtr &operator=(HypObjectPtr &&other) noexcept  = default;
    ~HypObjectPtr()                                         = default;

    HYP_FORCE_INLINE const HypClass *GetClass() const
        { return m_hyp_class; }

private:
    HYP_API const HypClass *GetHypClass(TypeID type_id) const;

    void            *m_ptr;
    const HypClass  *m_hyp_class;
};

} // namespace hyperion

#endif