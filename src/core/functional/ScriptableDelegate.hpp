/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCRIPTABLE_DELEGATE_HPP
#define HYPERION_SCRIPTABLE_DELEGATE_HPP

#include <core/functional/ScriptableDelegateFwd.hpp>
#include <core/functional/Delegate.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/object/managed/ManagedObjectResource.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <dotnet/Object.hpp>

namespace hyperion {

namespace functional {

HYP_API void LogScriptableDelegateError(const char* message, dotnet::Object* object_ptr);

class IScriptableDelegate : public virtual IDelegate
{
public:
    virtual ~IScriptableDelegate() = default;

    virtual DelegateHandler BindManaged(const String& method_name, Proc<ManagedObjectResource*()>&& get_fn) = 0;
    virtual DelegateHandler BindManaged(const String& method_name, ManagedObjectResource* managed_object_resource) = 0;
    virtual DelegateHandler BindManaged(const String& method_name, UniquePtr<dotnet::Object>&& object) = 0;
};

/*! \brief A delegate that can be bound to a managed .NET object.
 *  \details This delegate can be bound to a managed .NET object, allowing the delegate have its behavior defined in script code.
 *  \tparam ReturnType The return type of the delegate.
 *  \tparam Args The argument types of the delegate.
 *  \note The default return value can be changed by specializing the \ref{hyperion::functional::ProcDefaultReturn} struct. */
template <class ReturnType, class... Args>
class ScriptableDelegate final : public IScriptableDelegate, public virtual Delegate<ReturnType, Args...>
{
public:
    using ProcType = Proc<ReturnType(Args...)>;

    ScriptableDelegate() = default;
    ScriptableDelegate(const ScriptableDelegate& other) = delete;
    ScriptableDelegate& operator=(const ScriptableDelegate& other) = delete;

    ScriptableDelegate(ScriptableDelegate&& other) noexcept
        : Delegate<ReturnType, Args...>(std::move(other.m_delegate))
    {
    }

    ScriptableDelegate& operator=(ScriptableDelegate&& other) noexcept = delete;

    virtual ~ScriptableDelegate() override = default;

    HYP_NODISCARD virtual DelegateHandler BindManaged(const String& method_name, Proc<ManagedObjectResource*()>&& get_fn) override
    {
        if (!get_fn)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([method_name = method_name, get_fn = std::move(get_fn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                ManagedObjectResource* managed_object_resource = get_fn();
                AssertThrowMsg(managed_object_resource != nullptr, "Managed object resource is null!");

                managed_object_resource->IncRef();
                HYP_DEFER({ managed_object_resource->DecRef(); });

                dotnet::Object* object = managed_object_resource->GetManagedObject();
                AssertThrowMsg(object != nullptr, "Managed object is null!");
                AssertThrowMsg(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(method_name))
                {
                    HYP_FAIL("Failed to find method %s!", method_name.Data());
                }

                return object->InvokeMethodByName<ReturnType>(method_name, std::forward<ArgTypes>(args)...);
            });
    }

    template <class DefaultReturnType, typename = std::enable_if_t<std::is_copy_constructible_v<NormalizedType<DefaultReturnType>>>>
    HYP_NODISCARD DelegateHandler BindManaged(const String& method_name, Proc<ManagedObjectResource*()>&& get_fn, DefaultReturnType&& default_return)
    {
        if (!get_fn)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([method_name = method_name, get_fn = std::move(get_fn), default_return = std::forward<DefaultReturnType>(default_return)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                ManagedObjectResource* managed_object_resource = get_fn();
                AssertThrowMsg(managed_object_resource != nullptr, "Managed object resource is null!");

                managed_object_resource->IncRef();
                HYP_DEFER({ managed_object_resource->DecRef(); });

                dotnet::Object* object = managed_object_resource->GetManagedObject();
                AssertThrowMsg(object != nullptr, "Managed object is null!");
                AssertThrowMsg(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(method_name))
                {
                    return default_return;
                }

                return object->InvokeMethodByName<ReturnType>(method_name, std::forward<ArgTypes>(args)...);
            });
    }

    HYP_NODISCARD virtual DelegateHandler BindManaged(const String& method_name, ManagedObjectResource* managed_object_resource) override
    {
        if (!managed_object_resource)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([method_name = method_name, managed_object_resource]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                managed_object_resource->IncRef();
                HYP_DEFER({ managed_object_resource->DecRef(); });

                dotnet::Object* object = managed_object_resource->GetManagedObject();
                AssertThrowMsg(object != nullptr, "Managed object is null!");
                AssertThrowMsg(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(method_name))
                {
                    HYP_FAIL("Failed to find method %s!", method_name.Data());
                }

                return object->InvokeMethodByName<ReturnType>(method_name, std::forward<ArgTypes>(args)...);
            });
    }

    template <class DefaultReturnType, typename = std::enable_if_t<std::is_copy_constructible_v<NormalizedType<DefaultReturnType>>>>
    HYP_NODISCARD DelegateHandler BindManaged(const String& method_name, ManagedObjectResource* managed_object_resource, DefaultReturnType&& default_return)
    {
        if (!managed_object_resource)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([method_name = method_name, managed_object_resource, default_return = std::forward<DefaultReturnType>(default_return)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                managed_object_resource->IncRef();
                HYP_DEFER({ managed_object_resource->DecRef(); });

                dotnet::Object* object = managed_object_resource->GetManagedObject();
                AssertThrowMsg(object != nullptr, "Managed object is null!");
                AssertThrowMsg(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(method_name))
                {
                    return default_return;
                }

                return object->InvokeMethodByName<ReturnType>(method_name, std::forward<ArgTypes>(args)...);
            });
    }

    HYP_NODISCARD virtual DelegateHandler BindManaged(const String& method_name, UniquePtr<dotnet::Object>&& object) override
    {
        if (!object)
        {
            return DelegateHandler();
        }

        if (!object->IsValid())
        {
            LogScriptableDelegateError("Managed object is invalid!", object.Get());

            return DelegateHandler();
        }

        if (!object->SetKeepAlive(true))
        {
            LogScriptableDelegateError("Failed to set keep alive to true!", object.Get());

            return DelegateHandler();
        }

        if (!object->GetMethod(method_name))
        {
            LogScriptableDelegateError("Failed to find method!", object.Get());

            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([method_name = method_name, object = std::move(object)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                return object->InvokeMethodByName<ReturnType>(method_name, std::forward<ArgTypes>(args)...);
            });
    }

    /*! \brief Call operator overload - alias method for Broadcast().
     *  \note The default return value can be changed by specializing the \ref{hyperion::functional::ProcDefaultReturn} struct.
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes&&... args) const
    {
        return const_cast<ScriptableDelegate*>(this)->Broadcast(std::forward<ArgTypes>(args)...);
    }
};

} // namespace functional
} // namespace hyperion

#endif