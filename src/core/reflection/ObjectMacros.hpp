/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

namespace Hyperion {

class Class;

template <class T>
const Class* GetClass();

template <class T>
struct GetClassHelper
{
    static const Class* Get();
};

class ClassRegistrationBase;

template <class T>
class HYP_API TClassStaticInit final
{
public:
    TClassStaticInit(); // leave undefined to cause linker error if not specialized

    TClassStaticInit(const TClassStaticInit& other) = delete;
    TClassStaticInit& operator=(const TClassStaticInit& other) = delete;

    TClassStaticInit(TClassStaticInit&& other) noexcept = delete;
    TClassStaticInit& operator=(TClassStaticInit&& other) noexcept = delete;

    ~TClassStaticInit()
    {
        if (m_registration != nullptr)
        {
            delete m_registration;
            m_registration = nullptr;
        }
    }

    ClassRegistrationBase* GetClassRegistration()
    {
        return m_registration;
    }

protected:
    ClassRegistrationBase* m_registration;
};

/// Macro for class / struct / enum declaration ///

// clang-format off

#define HYP_BEGIN_STRUCT(cls, _static_index, _num_descendants, parentClass, ...)                                                                \
                                                                                                                                                \
    template <>                                                                                                                                 \
    HYP_API const Class* GetClassHelper<cls>::Get() { return g_cls##cls; }                                                                      \
                                                                                                                                                \
    template <>                                                                                                                                 \
    TClassStaticInit<cls>::TClassStaticInit()                                                                                                   \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
                                                                                                                                                \
        m_registration = new ::Hyperion::StructRegistration<cls> { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const ClassAttribute> { { __VA_ARGS__ } }, Span<MemberVariant> { {

#define HYP_END_STRUCT } } }; }

#define HYP_BEGIN_CLASS(cls, _static_index, _num_descendants, parentClass, ...)                                                                 \
                                                                                                                                                \
    template <>                                                                                                                                 \
    HYP_API const Class* GetClassHelper<cls>::Get() { return g_cls##cls; }                                                                      \
                                                                                                                                                \
    template <>                                                                                                                                 \
    TClassStaticInit<cls>::TClassStaticInit()                                                                                                   \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
                                                                                                                                                \
        m_registration = new ::Hyperion::ClassRegistration<cls> { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const ClassAttribute> { { __VA_ARGS__ } }, Span<MemberVariant> { {

#define HYP_END_CLASS } } }; }

#define HYP_BEGIN_ENUM(cls, _static_index, _num_descendants, ...)                                                                               \
                                                                                                                                                \
    template <>                                                                                                                                 \
    HYP_API const Class* GetClassHelper<cls>::Get() { return g_cls##cls; }                                                                      \
                                                                                                                                                \
    template <>                                                                                                                                 \
    TClassStaticInit<cls>::TClassStaticInit()                                                                                                   \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
                                                                                                                                                \
        m_registration = new ::Hyperion::EnumRegistration<cls> { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, Span<const ClassAttribute> { { __VA_ARGS__ } }, Span<MemberVariant> { {

#define HYP_END_ENUM } } }; }

// clang-format on

/// Macros for class / struct body ///

#define HYP_OBJECT_BODY(T, ...)                                                  \
private:                                                                         \
    template <class TStaticInitType>                                             \
    friend class TClassStaticInit;                                               \
                                                                                 \
public:                                                                          \
    struct ClassInfo                                                             \
    {                                                                            \
        using Type = T;                                                          \
    };                                                                           \
                                                                                 \
    HYP_FORCE_INLINE ObjId<T> Id() const                                         \
    {                                                                            \
        return (ObjId<T>)(ObjectBase::Id());                                     \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE static const Class* StaticClass()                           \
    {                                                                            \
        return Hyperion::GetClass<T>();                                          \
    }                                                                            \
                                                                                 \
    template <class TOther>                                                      \
    HYP_FORCE_INLINE bool IsA() const                                            \
    {                                                                            \
        if constexpr (std::is_same_v<T, TOther> || std::is_base_of_v<TOther, T>) \
        {                                                                        \
            return true;                                                         \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            static const Class* otherClass = TOther::StaticClass();              \
            if (!otherClass)                                                     \
            {                                                                    \
                return false;                                                    \
            }                                                                    \
            return Hyperion::IsA(otherClass, InstanceClass());                   \
        }                                                                        \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE bool IsA(const Class* otherClass) const                     \
    {                                                                            \
        if (!otherClass)                                                         \
        {                                                                        \
            return false;                                                        \
        }                                                                        \
        return Hyperion::IsA(otherClass, InstanceClass());                       \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE Handle<T> HandleFromThis() const                            \
    {                                                                            \
        Handle<T> handle = Handle<T>::FromPointer(const_cast<T*>(this));         \
                                                                                 \
        if (!handle)                                                             \
        {                                                                        \
            HYP_FAIL("HandleFromThis() called in destructor!");                  \
        }                                                                        \
                                                                                 \
        return handle;                                                           \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE WeakHandle<T> WeakHandleFromThis() const                    \
    {                                                                            \
        return WeakHandle<T>::FromPointer(const_cast<T*>(this));                 \
    }                                                                            \
                                                                                 \
private:

#define HYP_STRUCT_BODY(T, ...)                        \
    template <class TStaticInitType>                   \
    friend class TClassStaticInit;                     \
                                                       \
    struct ClassInfo                                   \
    {                                                  \
        using Type = T;                                \
    };                                                 \
                                                       \
    HYP_FORCE_INLINE static const Class* StaticClass() \
    {                                                  \
        return Hyperion::GetClass<T>();                \
    }

#define HYP_REGISTER_STATIC_CLASS(T) \
    static TClassStaticInit<T> s_classInit##T;

} // namespace Hyperion
