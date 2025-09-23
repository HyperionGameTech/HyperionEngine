/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/object/HypObjectBase.hpp>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass* GetClass(TypeId typeId);

#define HYP_OBJECT_BODY(T, ...)                                                  \
private:                                                                         \
    friend struct HypClassInitializer_##T;                                       \
                                                                                \
    template <class T>                                                          \
    friend class HypClassInstance;                                              \
                                                                                \
    template <class T>                                                          \
    friend struct HypClassRegistration;                                          \
                                                                                 \
public:                                                                          \
    struct HypClassInfo                                                         \
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
        static const HypClass* hypClass = GetClass(TypeId::ForType<T>());        \
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


#define HYP_STRUCT_BODY(T, ...)                                                     \
    friend struct HypStructInitializer_##T;                                     \
                                                                                \
    template <class T>                                                          \
    friend class HypStructInstance;                                             \
                                                                                \
    template <class T>                                                          \
    friend struct HypStructRegistration;                                        \
                                                                                \
    struct HypClassInfo                                                         \
    {                                                                           \
        using Type = T;                                                         \
    };                                                                          \
                                                                                 \
    HYP_FORCE_INLINE static const HypClass* Class()                              \
    {                                                                            \
        static const HypClass* hypClass = GetClass(TypeId::ForType<T>());        \
        return hypClass;                                                         \
    }
    


} // namespace hyperion
