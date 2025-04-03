/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCRIPTABLE_DELEGATE_HPP
#define HYPERION_SCRIPTABLE_DELEGATE_HPP

#include <core/functional/ScriptableDelegateFwd.hpp>
#include <core/functional/Delegate.hpp>

#include <dotnet/Object.hpp>

namespace hyperion {

namespace functional {

class IScriptableDelegate : public virtual IDelegate
{
public:
    virtual ~IScriptableDelegate() = default;

    virtual DelegateHandler BindManaged(UniquePtr<dotnet::Object> &&delegate_object_ptr) = 0;
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
    using ProcType = Proc<ReturnType(Args...)>;

    ScriptableDelegate()                                                = default;
    ScriptableDelegate(const ScriptableDelegate &other)                 = delete;
    ScriptableDelegate &operator=(const ScriptableDelegate &other)      = delete;

    ScriptableDelegate(ScriptableDelegate &&other) noexcept
        : Delegate<ReturnType, Args...>(std::move(other.m_delegate))
    {
    }

    ScriptableDelegate &operator=(ScriptableDelegate &&other) noexcept  = delete;

    virtual ~ScriptableDelegate() override                              = default;

    HYP_NODISCARD virtual DelegateHandler BindManaged(UniquePtr<dotnet::Object> &&delegate_object_ptr) override
    {
        return Delegate<ReturnType, Args...>::Bind([delegate_object_ptr = std::move(delegate_object_ptr)]<class... ArgTypes>(ArgTypes &&... args) mutable -> ReturnType
        {
            return delegate_object_ptr->InvokeMethodByName<ReturnType>("Invoke", std::forward<ArgTypes>(args)...);
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

} // namespace functional
} // namespace hyperion

#endif