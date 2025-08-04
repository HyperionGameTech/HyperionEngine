/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/object/HypObjectBase.hpp>

namespace hyperion {

#define HYP_OBJECT_BODY(T, ...)                                                  \
private:                                                                         \
    friend class HypObjectInitializer<T>;                                        \
    friend struct HypClassInitializer_##T;                                       \
    friend class HypClassInstance<T>;                                            \
    friend struct HypClassRegistration<T>;                                       \
                                                                                 \
    HypObjectInitializer<T> m_hypObjectInitializer { this };                     \
    IHypObjectInitializer* m_hypObjectInitializerPtr = &m_hypObjectInitializer;  \
                                                                                 \
public:                                                                          \
    struct HypObjectData                                                         \
    {                                                                            \
        using Type = T;                                                          \
                                                                                 \
        static constexpr bool isHypObject = true;                                \
    };                                                                           \
                                                                                 \
    HYP_FORCE_INLINE ObjId<T> Id() const                                         \
    {                                                                            \
        return (ObjId<T>)(HypObjectBase::Id());                                  \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE IHypObjectInitializer* GetObjectInitializer() const         \
    {                                                                            \
        return m_hypObjectInitializerPtr;                                        \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE static const HypClass* Class()                              \
    {                                                                            \
        return HypObjectInitializer<T>::GetClass_Static();                       \
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
        return Handle<T>::FromPointer(const_cast<T*>(this));                     \
    }                                                                            \
                                                                                 \
    HYP_FORCE_INLINE WeakHandle<T> WeakHandleFromThis() const                    \
    {                                                                            \
        return WeakHandle<T>::FromPointer(const_cast<T*>(this));                 \
    }                                                                            \
                                                                                 \
private:

} // namespace hyperion
