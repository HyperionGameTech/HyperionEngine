/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_COMPONENTS_BASE_HPP
#define HYPERION_COMPONENTS_BASE_HPP

#include <core/Core.hpp>
#include <core/ObjectPool.hpp>
#include <core/ID.hpp>
#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/StaticString.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/functional/Delegate.hpp>

#include <core/threading/AtomicVar.hpp>

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

class BasicObjectBase
{
};

template <class T>
struct BasicObjectClassInfo
{
    static constexpr TypeID type_id = TypeID::ForType<T>();

    HYP_FORCE_INLINE const HypClass *GetClass() const
    {
        return ::hyperion::GetClass(type_id);
    }
};

template <class T>
class BasicObject : public BasicObjectBase
{
    using InnerType = T;

    static constexpr auto type_name = TypeNameWithoutNamespace<InnerType>();
    
    static constexpr BasicObjectClassInfo<InnerType> class_info = { };

public:
    using InitInfo = ComponentInitInfo<InnerType>;

    static constexpr ID<InnerType> empty_id = ID<InnerType> { };

    enum InitState : uint16
    {
        INIT_STATE_UNINITIALIZED    = 0x0,
        INIT_STATE_INIT_CALLED      = 0x1,
        INIT_STATE_READY            = 0x2
    };

    BasicObject()
        : BasicObject(InitInfo { })
    {
    }

    BasicObject(Name name)
        : BasicObject(name, InitInfo { })
    {
    }

    BasicObject(const InitInfo &init_info)
        : BasicObject(Name::Invalid(), init_info)
    {
    }

    BasicObject(Name name, const InitInfo &init_info)
        : m_name(name),
          m_init_info(init_info),
          m_id(empty_id),
          m_init_state(INIT_STATE_UNINITIALIZED)
    {
    }

    BasicObject(const BasicObject &other)               = delete;
    BasicObject &operator=(const BasicObject &other)    = delete;

    BasicObject(BasicObject &&other) noexcept
        : m_name(std::move(other.m_name)),
          m_init_info(std::move(other.m_init_info)),
          m_id(other.m_id),
          m_init_state(other.m_init_state.Get(MemoryOrder::RELAXED)),
          m_managed_object(std::move(other.m_managed_object))
    {
        other.m_id = empty_id;
        other.m_init_state.Set(INIT_STATE_UNINITIALIZED, MemoryOrder::RELAXED);
    }

    BasicObject &operator=(BasicObject &&other) noexcept = delete;

    ~BasicObject() = default;

    HYP_FORCE_INLINE InitInfo &GetInitInfo()
        { return m_init_info; }

    HYP_FORCE_INLINE const InitInfo &GetInitInfo() const
        { return m_init_info; }

    HYP_FORCE_INLINE bool HasFlags(ComponentFlags flags) const
        { return bool(m_init_info.flags & flags); }

    HYP_FORCE_INLINE void SetFlags(ComponentFlags flags, bool enable = true)
        { m_init_info.flags = enable ? (m_init_info.flags | flags) : (m_init_info.flags & ~flags); }

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

    HYP_FORCE_INLINE dotnet::Object *GetManagedObject() const
        { return m_managed_object.Get(); }

    HYP_FORCE_INLINE static const BasicObjectClassInfo<InnerType> &GetClassInfo()
        { return class_info; }

    HYP_FORCE_INLINE static const HypClass *GetClass()
        { return class_info.GetClass(); }

    void Init()
    {
        m_init_state.BitOr(INIT_STATE_INIT_CALLED, MemoryOrder::RELAXED);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type_name);
        hc.Add(m_id);

        return hc;
    }

protected:
    using Base = BasicObject<T>;
    
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
            "Object of type `%s` is not in ready state; maybe Init() has not been called on it, "
            "or the component requires an event to be sent from the Engine instance to determine that "
            "it is ready to be constructed, and this event has not yet been sent.\n",
            TypeNameWithoutNamespace<InnerType>().Data()
        );
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        AssertThrowMsg(
            IsInitCalled(),
            "Object of type `%s` has not had Init() called on it!\n",
            TypeNameWithoutNamespace<InnerType>().Data()
        );
    }

    void AddDelegateHandler(DelegateHandler &&delegate_handler)
    {
        m_delegate_handlers.PushBack(std::move(delegate_handler));
    }
    
    ID<InnerType>               m_id;
    Name                        m_name;
    AtomicVar<uint16>           m_init_state;
    InitInfo                    m_init_info;
    Array<DelegateHandler>      m_delegate_handlers;
    UniquePtr<dotnet::Object>   m_managed_object;
};

} // namespace hyperion

#endif // !HYPERION_COMPONENTS_BASE_HPP

