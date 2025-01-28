/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCRIPTABLE_DELEGATE_HPP
#define HYPERION_SCRIPTABLE_DELEGATE_HPP

#include <core/functional/Delegate.hpp>

#include <dotnet/Object.hpp>

namespace hyperion {

namespace functional {

class IScriptableDelegate : public virtual IDelegate
{
public:
    virtual ~IScriptableDelegate() = default;

    virtual DelegateHandler BindManaged(dotnet::Object &&delegate_object) = 0;
};

/*! \brief A delegate that can be bound to a managed .NET object.
 *  \details This delegate can be bound to a managed .NET object, allowing the delegate have its behavior defined in script code.
 *  \tparam ReturnType The return type of the delegate.
 *  \tparam Args The argument types of the delegate.
 *  \note The default return value can be changed by specializing the \ref{hyperion::functional::detail::ProcDefaultReturn} struct. */
template <class ReturnType, class... Args>
class ScriptableDelegate final : public IScriptableDelegate, public virtual Delegate<ReturnType, Args...>
{
public:
    using ProcType = Proc<ReturnType, Args...>;

    ScriptableDelegate()                                                = default;
    ScriptableDelegate(const ScriptableDelegate &other)                 = delete;
    ScriptableDelegate &operator=(const ScriptableDelegate &other)      = delete;

    ScriptableDelegate(ScriptableDelegate &&other) noexcept
        : Delegate<ReturnType, Args...>(std::move(other.m_delegate))
    {
    }

    ScriptableDelegate &operator=(ScriptableDelegate &&other) noexcept  = delete;

    virtual ~ScriptableDelegate() override                              = default;

    virtual DelegateHandler BindManaged(dotnet::Object &&delegate_object) override
    {
        return Delegate<ReturnType, Args...>::Bind([object = std::move(delegate_object)]<class... ArgTypes>(ArgTypes &&... args) mutable -> ReturnType
        {
            return object.InvokeMethodByName<ReturnType>("Invoke", std::forward<ArgTypes>(args)...);
        });
    }
    /*! \brief Call operator overload - alias method for Broadcast().
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::detail::ProcDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes &&... args) const
        { return const_cast<ScriptableDelegate *>(this)->Broadcast(std::forward<ArgTypes>(args)...); }
};

// template <class ReturnType, class... Args>
// class ScriptableDelegate final : public IScriptableDelegate
// {
// public:
//     using ProcType = Proc<ReturnType, Args...>;

//     ScriptableDelegate()                                                = default;
//     ScriptableDelegate(const ScriptableDelegate &other)                 = delete;
//     ScriptableDelegate &operator=(const ScriptableDelegate &other)      = delete;

//     ScriptableDelegate(ScriptableDelegate &&other) noexcept
//         : m_delegate(std::move(other.m_delegate))
//     {
//     }

//     ScriptableDelegate &operator=(ScriptableDelegate &&other) noexcept  = delete;

//     virtual ~ScriptableDelegate() override                               = default;

//     HYP_FORCE_INLINE bool operator!() const
//         { return !AnyBound(); }

//     HYP_FORCE_INLINE explicit operator bool() const
//         { return AnyBound(); }

//     virtual bool AnyBound() const override
//         { return m_delegate.AnyBound(); }

//     DelegateHandler BindOwned(const void *owner, ProcType &&proc, bool require_current_thread = false)
//     {
//         return m_delegate.BindOwned(owner, std::move(proc), require_current_thread);
//     }

//     DelegateHandler Bind(ProcType &&proc, bool require_current_thread = false)
//     {
//         return m_delegate.Bind(std::move(proc), require_current_thread);
//     }

//     DelegateHandler Bind(ProcType &&proc, ThreadID calling_thread_id, const void *owner = nullptr)
//     {
//         return m_delegate.Bind(std::move(proc), calling_thread_id, owner);
//     }

//     virtual DelegateHandler BindManaged(dotnet::Object &&delegate_object) override
//     {
//         return m_delegate.Bind(MakeRefCountedPtr<ProcType>([object = std::move(delegate_object)]<class... ArgTypes>(ArgTypes &&... args) mutable -> ReturnType
//         {
//             return object.InvokeMethodByName<ReturnType>("Invoke", std::forward<ArgTypes>(args)...);
//         }));
//     }

//     virtual int RemoveAll(bool thread_safe = true) override
//         { return m_delegate.RemoveAll(thread_safe); }

//     virtual int RemoveAll(const void *owner, bool thread_safe = true) override
//         { return m_delegate.RemoveAll(owner, thread_safe); }

//     virtual bool Remove(const DelegateHandler &handler) override
//         { return m_delegate.Remove(handler); }

//     virtual bool Remove(uint32 id) override
//         { return m_delegate.Remove(id); }

//     template <class... ArgTypes>
//     ReturnType Broadcast(ArgTypes &&... args)
//         { return m_delegate.Broadcast(std::forward<ArgTypes>(args)...); }

//     template <class... ArgTypes>
//     HYP_FORCE_INLINE ReturnType operator()(ArgTypes &&... args) const
//         { return const_cast<ScriptableDelegate *>(this)->Broadcast(std::forward<ArgTypes>(args)...); }

// private:
//     Delegate<ReturnType, Args...>   m_delegate;
// };

} // namespace functional

using functional::IScriptableDelegate;
using functional::ScriptableDelegate;

} // namespace hyperion

#endif