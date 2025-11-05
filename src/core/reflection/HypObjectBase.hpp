/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/reflection/ObjId.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/debug/Debug.hpp>

#include <type_traits>

namespace hyperion {

class Class;
class ScriptObjectResource;
struct HypObjectHeader;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

namespace utilities {
struct TypeId;
} // namespace utilities

using utilities::TypeId;

#ifdef HYP_DOTNET
namespace dotnet {
class ManagedClass;
} // namespace dotnet
#endif

HYP_API extern bool IsA(const Class* cls, const void* ptr, const TypeId& typeId);
HYP_API extern bool IsA(const Class* cls, const Class* instanceClass);

class HYP_API HypObjectBase
{
    template <class T>
    friend struct Handle;

    template <class T>
    friend struct WeakHandle;

    friend struct AnyHandle;

    template <class T, class... Args>
    friend Handle<T> CreateObject(Args&&...);

    template <class T>
    friend bool InitObject(const Handle<T>&);

public:
    struct ClassInfo
    {
        using Type = HypObjectBase;
    };

    /*! \internal */
    HypObjectBase();

    virtual ~HypObjectBase();

    ObjIdBase Id() const;

    static const Class* StaticClass();
    const Class* InstanceClass() const;

    template <class TOther>
    HYP_FORCE_INLINE bool IsA() const
    {
        if constexpr (std::is_same_v<HypObjectBase, TOther>)
        {
            return true;
        }
        else
        {
            static const Class* otherClass = TOther::StaticClass();

            if (!otherClass)
            {
                return false;
            }

            return hyperion::IsA(otherClass, InstanceClass());
        }
    }

    HYP_FORCE_INLINE bool IsA(const Class* cls) const
    {
        return hyperion::IsA(cls, InstanceClass());
    }

    HYP_FORCE_INLINE HypObjectHeader* GetObjectHeader_Internal() const
    {
        return m_header;
    }

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    void SetScriptObjectResource(ScriptObjectResource* scriptObjectResource)
    {
        HYP_CORE_ASSERT(m_scriptObjectResource == nullptr);

        m_scriptObjectResource = scriptObjectResource;
    }

    ScriptObjectResource* GetScriptObjectResource() const
    {
        return m_scriptObjectResource;
    }

#ifdef HYP_DOTNET
    dotnet::ManagedObject* GetManagedObject() const;
#endif
#endif

    HYP_FORCE_INLINE bool IsInitCalled() const
    {
        return m_initState.Get(MemoryOrder::RELAXED) & INIT_STATE_INIT_CALLED;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_initState.Get(MemoryOrder::RELAXED) & INIT_STATE_READY;
    }

protected:
    enum InitState : uint16
    {
        INIT_STATE_UNINITIALIZED = 0x0,
        INIT_STATE_INIT_CALLED = 0x1,
        INIT_STATE_READY = 0x2
    };

    HypObjectBase(const HypObjectBase& other) = delete;
    HypObjectBase& operator=(const HypObjectBase& other) = delete;

    HypObjectBase(HypObjectBase&& other) noexcept
        : m_initState(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    HypObjectBase& operator=(HypObjectBase&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_initState.Set(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

        return *this;
    }

    /*! \brief Don't call manually (except for instances of derived types calling base class Init() in their own Init() method). */
    virtual void Init()
    {
        // Do nothing by default.
    }

    void SetReady(bool isReady)
    {
        if (isReady)
        {
            m_initState.BitOr(INIT_STATE_READY, MemoryOrder::RELAXED);
        }
        else
        {
            m_initState.BitAnd(~INIT_STATE_READY, MemoryOrder::RELAXED);
        }
    }

    HYP_FORCE_INLINE void AssertReady() const
    {
        HYP_CORE_ASSERT(IsReady(), "Object is not in ready state! Was InitObject() called for it?");
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        HYP_CORE_ASSERT(IsInitCalled(), "Object has not had Init() called on it!");
    }

    // Pointer to the header of the object, holding container, index and ref counts. Must be the first member.
    HypObjectHeader* m_header;

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
    ScriptObjectResource* m_scriptObjectResource;
#endif

private:
    // Used internally by InitObject() to call derived Init() methods.
    void Init_Internal()
    {
        Init();
    }

    AtomicVar<uint16> m_initState;
};

} // namespace hyperion
