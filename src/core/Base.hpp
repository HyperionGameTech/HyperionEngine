/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_COMPONENTS_BASE_HPP
#define HYPERION_COMPONENTS_BASE_HPP

#include <core/ID.hpp>
#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/StaticString.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/AtomicVar.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

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

class HYP_API BasicObjectBase
{
protected:
    BasicObjectBase() = default;
    BasicObjectBase(BasicObjectBase &&other) noexcept;
    ~BasicObjectBase();
};

template <class T>
class BasicObject : public BasicObjectBase
{
    using InnerType = T;

public:
    enum InitState : uint16
    {
        INIT_STATE_UNINITIALIZED    = 0x0,
        INIT_STATE_INIT_CALLED      = 0x1,
        INIT_STATE_READY            = 0x2
    };

    BasicObject()
        : BasicObject(Name::Invalid())
    {
    }

    BasicObject(Name name)
        : m_name(name),
          m_init_state(INIT_STATE_UNINITIALIZED)
    {
    }

    BasicObject(const BasicObject &other)               = delete;
    BasicObject &operator=(const BasicObject &other)    = delete;

    BasicObject(BasicObject &&other) noexcept
        : m_name(std::move(other.m_name)),
          m_id(other.m_id),
          m_init_state(other.m_init_state.Get(MemoryOrder::RELAXED))
    {
        other.m_id = ID<T> { };
        other.m_init_state.Set(INIT_STATE_UNINITIALIZED, MemoryOrder::RELAXED);
    }

    BasicObject &operator=(BasicObject &&other) noexcept = delete;

    ~BasicObject() = default;

    HYP_FORCE_INLINE ID<InnerType> GetID() const
        { return m_id; }

    /* To be called from ObjectHolder<Type> */
    HYP_FORCE_INLINE void SetID(ID<InnerType> id)
        { m_id = id; }

    /*! \brief Get assigned name of the object */
    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    /*! \brief Set the assigned name of the object */
    HYP_FORCE_INLINE void SetName(Name name)
        { m_name = name; }

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

protected:
    HYP_FORCE_INLINE Handle<T> HandleFromThis() const
    {
        return Handle<T>(m_id);
    }

    HYP_FORCE_INLINE WeakHandle<T> WeakHandleFromThis() const
    {
        return WeakHandle<T>(m_id);
    }

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
    
    ID<InnerType>               m_id;
    Name                        m_name;
    AtomicVar<uint16>           m_init_state;
    Array<DelegateHandler>      m_delegate_handlers;
};

// template <class T>
// const ClassInfo<T> BasicObject<T>::s_class_info = { };

} // namespace hyperion

#endif // !HYPERION_COMPONENTS_BASE_HPP

