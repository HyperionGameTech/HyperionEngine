/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>
#include <core/object/HypObjectEnums.hpp>
#include <core/object/ObjId.hpp>
#include <core/object/HypObjectPool.hpp>

#if defined(HYP_DOTNET) || defined(HYP_SCRIPT)
#include <scripting/ScriptObjectResource.hpp>
#endif

#include <core/utilities/TypeId.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/functional/Delegate.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;
struct HypData;

template <class T>
class HypClassInstance;

template <class T>
struct HypClassDefinition;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

namespace dotnet {
class Class;
} // namespace dotnet

extern HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const HypClass* instanceHypClass);

extern HYP_API TypeId GetTypeIdForClass(const HypClass* hypClass);

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
    struct HypObjectData
    {
        using Type = HypObjectBase;
    };

    /*! \internal */
    HypObjectBase();

    virtual ~HypObjectBase();

    HYP_FORCE_INLINE ObjIdBase Id() const
    {
        HYP_CORE_ASSERT(m_header, "Invalid HypObject!");

        return ObjIdBase { GetTypeIdForClass(m_header->hypClass), m_header->index + 1 };
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        HYP_CORE_ASSERT(m_header, "Invalid HypObject!");

        return GetTypeIdForClass(m_header->hypClass);
    }

    HYP_FORCE_INLINE static const HypClass* Class()
    {
        return nullptr;
    }

    HYP_FORCE_INLINE const HypClass* InstanceClass() const
    {
        HYP_CORE_ASSERT(m_header, "Invalid HypObject!");
        HYP_CORE_ASSERT(m_header->hypClass, "No HypClass defined for type");

        return m_header->hypClass;
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

    dotnet::Object* GetManagedObject() const
    {
        return m_scriptObjectResource ? m_scriptObjectResource->GetManagedObject() : nullptr;
    }
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
        : m_initState(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE)),
          m_delegateHandlers(std::move(other.m_delegateHandlers))
    {
    }

    HypObjectBase& operator=(HypObjectBase&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_initState.Set(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);
        m_delegateHandlers = std::move(other.m_delegateHandlers);

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

    void AddDelegateHandler(Name name, DelegateHandler&& delegateHandler)
    {
        m_delegateHandlers.Add(name, std::move(delegateHandler));
    }

    void AddDelegateHandler(DelegateHandler&& delegateHandler)
    {
        m_delegateHandlers.Add(std::move(delegateHandler));
    }

    bool RemoveDelegateHandler(WeakName name)
    {
        return m_delegateHandlers.Remove(name);
    }

    // Pointer to the header of the object, holding container, index and ref counts. Must be the first member.
    HypObjectHeader* m_header;
    DelegateHandlerSet m_delegateHandlers;

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
