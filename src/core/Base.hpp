/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_COMPONENTS_BASE_HPP
#define HYPERION_COMPONENTS_BASE_HPP

#include <core/ID.hpp>
#include <core/Name.hpp>
#include <core/Defines.hpp>
#include <core/Handle.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/StaticString.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;

namespace dotnet {
class Object;
} // namespace dotnet

using ComponentFlags = uint;

template <class T>
struct ComponentInitInfo
{
    enum Flags : ComponentFlags {};

    ComponentFlags flags = 0x0;
};

template <class T>
class HypObject : public HypObjectBase
{
    using InnerType = T;

public:
    enum InitState : uint16
    {
        INIT_STATE_UNINITIALIZED    = 0x0,
        INIT_STATE_INIT_CALLED      = 0x1,
        INIT_STATE_READY            = 0x2
    };

    HypObject()
        : m_init_state(INIT_STATE_UNINITIALIZED)
    {
    }

    HypObject(const HypObject &other)               = delete;
    HypObject &operator=(const HypObject &other)    = delete;

    HypObject(HypObject &&other) noexcept
        : m_init_state(other.m_init_state.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    HypObject &operator=(HypObject &&other) noexcept = delete;

    virtual ~HypObject() override = default;

    HYP_FORCE_INLINE ID<InnerType> GetID() const
        { return ID<InnerType>(HypObjectBase::GetID().Value()); }

    HYP_FORCE_INLINE bool IsInitCalled() const
        { return m_init_state.Get(MemoryOrder::RELAXED) & INIT_STATE_INIT_CALLED; }

    HYP_FORCE_INLINE bool IsReady() const
        { return m_init_state.Get(MemoryOrder::RELAXED) & INIT_STATE_READY; }

    // HYP_FORCE_INLINE static const HypClass *GetClass()
    //     { return s_class_info.GetClass(); }

    void Init()
    {
        m_init_state.BitOr(INIT_STATE_INIT_CALLED, MemoryOrder::RELAXED);
    }

    HYP_FORCE_INLINE Handle<T> HandleFromThis() const
        { return Handle<T>(GetObjectHeader_Internal()); }

    HYP_FORCE_INLINE WeakHandle<T> WeakHandleFromThis() const
        { return WeakHandle<T>(GetObjectHeader_Internal());  }

protected:
    void SetReady(bool is_ready)
    {
        if (is_ready) {
            m_init_state.BitOr(INIT_STATE_READY, MemoryOrder::RELAXED);
        } else {
            m_init_state.BitAnd(~INIT_STATE_READY, MemoryOrder::RELAXED);
        }
    }

    HYP_FORCE_INLINE void AssertReady() const
    {
        AssertThrowMsg(
            IsReady(),
            "Object is not in ready state; maybe Init() has not been called on it, "
            "or the component requires an event to be sent from the Engine instance to determine that "
            "it is ready to be constructed, and this event has not yet been sent.\n"
        );
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        AssertThrowMsg(
            IsInitCalled(),
            "Object has not had Init() called on it!\n"
        );
    }

    void AddDelegateHandler(DelegateHandler &&delegate_handler)
    {
        m_delegate_handlers.PushBack(std::move(delegate_handler));
    }
    
    AtomicVar<uint16>           m_init_state;
    Array<DelegateHandler>      m_delegate_handlers;
};

} // namespace hyperion

#endif // !HYPERION_COMPONENTS_BASE_HPP

