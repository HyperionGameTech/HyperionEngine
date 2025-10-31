/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

namespace hyperion {

class HypClass;

template <class T>
const HypClass* GetClass();

/// Macro for class / struct / enum declaration ///

// clang-format off

#define HYP_BEGIN_STRUCT(cls, _static_index, _num_descendants, parentClass, ...)                                                                \
    HYP_API extern const HypClass* g_cls##cls;                                                                                                  \
                                                                                                                                                \
    static struct HypClassInitializer_##cls                                                                                                     \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::HypStructRegistration<Type>;                                                                       \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        HypClassInitializer_##cls ()                                                                                                            \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_STRUCT \
    }                  \
    }                  \
    };                 \

#define HYP_BEGIN_CLASS(cls, _static_index, _num_descendants, parentClass, ...)                                                                 \
    HYP_API extern const HypClass* g_cls##cls;                                                                                                  \
                                                                                                                                                \
    static struct HypClassInitializer_##cls                                                                                                     \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::HypClassRegistration<Type>;                                                                        \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        HypClassInitializer_##cls ()                                                                                                            \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_CLASS \
    }                 \
    }                 \
    };                \

#define HYP_BEGIN_ENUM(cls, _static_index, _num_descendants, ...)                                                                               \
    HYP_API extern const HypClass* g_cls##cls;                                                                                                  \
                                                                                                                                                \
    static struct HypClassInitializer_##cls                                                                                                     \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::HypEnumRegistration<Type>;                                                                        \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        HypClassInitializer_##cls ()                                                                                                            \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_ENUM \
    }                \
    }                \
    };

// clang-format on

/// Macros for class / struct body ///

#define HYP_OBJECT_BODY(T, ...)                                                  \
private:                                                                         \
    friend struct HypClassInitializer_##T;                                       \
                                                                                 \
public:                                                                          \
    struct HypClassInfo                                                          \
    {                                                                            \
        using Type = T;                                                          \
    };                                                                           \
                                                                                 \
    HYP_FORCE_INLINE ObjId<T> Id() const                                         \
    {                                                                            \
        return (ObjId<T>)(HypObjectBase::Id());                                  \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE static const HypClass* Class()                              \
    {                                                                            \
        static const HypClass* hypClass = GetClass<T>();                         \
        return hypClass;                                                         \
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
            static const HypClass* otherHypClass = TOther::Class();              \
            if (!otherHypClass)                                                  \
            {                                                                    \
                return false;                                                    \
            }                                                                    \
            return hyperion::IsA(otherHypClass, InstanceClass());                \
        }                                                                        \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE bool IsA(const HypClass* otherHypClass) const               \
    {                                                                            \
        if (!otherHypClass)                                                      \
        {                                                                        \
            return false;                                                        \
        }                                                                        \
        return hyperion::IsA(otherHypClass, InstanceClass());                    \
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

#define HYP_STRUCT_BODY(T, ...)                          \
    friend struct HypClassInitializer_##T;               \
                                                         \
    struct HypClassInfo                                  \
    {                                                    \
        using Type = T;                                  \
    };                                                   \
                                                         \
    HYP_FORCE_INLINE static const HypClass* Class()      \
    {                                                    \
        static const HypClass* hypClass = GetClass<T>(); \
        return hypClass;                                 \
    }

} // namespace hyperion
