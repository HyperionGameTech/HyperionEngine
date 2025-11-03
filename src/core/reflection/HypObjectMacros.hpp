/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

namespace hyperion {

class Class;

template <class T>
const Class* GetClass();

template <class T>
struct GetClassHelper
{
    static const Class* Get();
};

/// Macro for class / struct / enum declaration ///

// clang-format off

#define HYP_BEGIN_STRUCT(cls, _static_index, _num_descendants, parentClass, ...)                                                                \
    HYP_API extern const Class* g_cls##cls;                                                                                                     \
                                                                                                                                                \
    template <>                                                                                                                                 \
    static HYP_API const Class* GetClassHelper<cls>::Get() { return g_cls##cls; }                                                               \
                                                                                                                                                \
    static struct ClassInitializer_##cls                                                                                                        \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::StructRegistration<Type>;                                                                          \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        ClassInitializer_##cls ()                                                                                                               \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    ClassInitializer_##cls::RegistrationType ClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const ClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_STRUCT \
    }                  \
    }                  \
    };                 \

#define HYP_BEGIN_CLASS(cls, _static_index, _num_descendants, parentClass, ...)                                                                 \
    HYP_API extern const Class* g_cls##cls;                                                                                                     \
                                                                                                                                                \
    template <>                                                                                                                                 \
    static HYP_API const Class* GetClassHelper<cls>::Get() { return g_cls##cls; }                                                               \
                                                                                                                                                \
    static struct ClassInitializer_##cls                                                                                                        \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::ClassRegistration<Type>;                                                                           \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        ClassInitializer_##cls ()                                                                                                               \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    ClassInitializer_##cls::RegistrationType ClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const ClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_CLASS \
    }                 \
    }                 \
    };                \

#define HYP_BEGIN_ENUM(cls, _static_index, _num_descendants, ...)                                                                               \
    HYP_API extern const Class* g_cls##cls;                                                                                                     \
                                                                                                                                                \
    template <>                                                                                                                                 \
    static HYP_API const Class* GetClassHelper<cls>::Get() { return g_cls##cls; }                                                               \
                                                                                                                                                \
    static struct ClassInitializer_##cls                                                                                                        \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::EnumRegistration<Type>;                                                                            \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        ClassInitializer_##cls ()                                                                                                               \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    ClassInitializer_##cls::RegistrationType ClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, Span<const ClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_ENUM \
    }                \
    }                \
    };

// clang-format on

/// Macros for class / struct body ///

#define HYP_OBJECT_BODY(T, ...)                                                  \
private:                                                                         \
    friend struct ClassInitializer_##T;                                          \
                                                                                 \
public:                                                                          \
    struct ClassInfo                                                             \
    {                                                                            \
        using Type = T;                                                          \
    };                                                                           \
                                                                                 \
    HYP_FORCE_INLINE ObjId<T> Id() const                                         \
    {                                                                            \
        return (ObjId<T>)(HypObjectBase::Id());                                  \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE static const Class* StaticClass()                           \
    {                                                                            \
        return hyperion::GetClass<T>();                                          \
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
            return hyperion::IsA(otherClass, InstanceClass());                   \
        }                                                                        \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE bool IsA(const Class* otherClass) const                     \
    {                                                                            \
        if (!otherClass)                                                         \
        {                                                                        \
            return false;                                                        \
        }                                                                        \
        return hyperion::IsA(otherClass, InstanceClass());                       \
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
    friend struct ClassInitializer_##T;                \
                                                       \
    struct ClassInfo                                   \
    {                                                  \
        using Type = T;                                \
    };                                                 \
                                                       \
    HYP_FORCE_INLINE static const Class* StaticClass() \
    {                                                  \
        return hyperion::GetClass<T>();                \
    }

} // namespace hyperion
