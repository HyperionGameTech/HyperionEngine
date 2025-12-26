/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/functional/ScriptableDelegateFwd.hpp>
#include <core/functional/Delegate.hpp>

#include <core/memory/resource/Resource.hpp>

#include <scripting/ScriptObjectResource.hpp>

#include <core/utilities/DeferredScope.hpp>

#ifdef HYP_DOTNET
#include <dotnet/ManagedObject.hpp>
#endif

namespace Hyperion {

class Method;
class ObjectBase;
struct BoxedValue;

extern "C" Method* Class_GetMethod(const Class* cls, const Name* methodName);

namespace functional {

HYP_API void LogScriptableDelegateError(const char* message, dotnet::ManagedObject* objectPtr);

class IScriptableDelegate : public virtual IDelegate
{
public:
    virtual ~IScriptableDelegate() = default;

    virtual HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn) = 0;
    virtual HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, ScriptObjectResource* scriptObjectResource) = 0;

#ifdef HYP_DOTNET
    virtual HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, UniquePtr<dotnet::ManagedObject>&& object) = 0;
#endif
};

class ScriptableDelegateHelper
{
public:
    static void InvokeMethod_Internal(BoxedValue* pOutBoxed, const Method* method, const Handle<ObjectBase>& target, Span<BoxedValue> argsHypData);

    template <class HypDataType, class ReturnType, class... Args>
    static bool InvokeScriptObjectMethod(ScriptObjectResource* scriptObjectResource, ANSIStringView methodName, ReturnType* outReturn, Args&&... args)
    {
        HYP_CORE_ASSERT(scriptObjectResource != nullptr, "Script object resource is null!");

        scriptObjectResource->IncRef();
        HYP_DEFER({ scriptObjectResource->DecRef(); });

#ifdef HYP_DOTNET
        if (scriptObjectResource->GetScriptLanguageMask() & (1u << SL_CSHARP))
        {
            dotnet::ManagedObject* object = scriptObjectResource->GetManagedObject();
            HYP_CORE_ASSERT(object != nullptr, "Managed object is null!");
            HYP_CORE_ASSERT(object->IsValid(), "Managed object is invalid!");

            if (object->GetMethod(methodName))
            {
                if constexpr (std::is_void_v<ReturnType>)
                {
                    object->InvokeMethodByName<void>(methodName, std::forward<Args>(args)...);

                    return true;
                }
                else
                {
                    new (outReturn) ReturnType(object->InvokeMethodByName<ReturnType>(methodName, std::forward<Args>(args)...));

                    return true;
                }
            }
        }
#endif

#ifdef HYP_SCRIPT
        if (scriptObjectResource->GetScriptLanguageMask() & (1u << SL_HYPSCRIPT))
        {
            ScriptObjectData_HypScript* hypScriptData = scriptObjectResource->GetScriptObjectData_HypScript();
            HYP_CORE_ASSERT(hypScriptData != nullptr, "HypScript script object data is null!");

            Script_Instance* instance = hypScriptData->instance;
            HYP_CORE_ASSERT(instance != nullptr, "HypScript instance is null!");

            HYP_NOT_IMPLEMENTED(); // see EntityScripting.cpp
        }
#endif

        ScriptObjectData_Native* nativeData = scriptObjectResource->GetScriptObjectData_Native();
        if (!nativeData)
        {
            return false;
        }

        Handle<ObjectBase> nativeObject = nativeData->nativeObject.Lock();
        if (!nativeObject)
        {
            return false;
        }

        const Name name = Name(StringHash(*methodName));

        const Method* method = Class_GetMethod(nativeObject->InstanceClass(), &name);
        if (!method)
        {
            return false;
        }

        return InvokeMethod<BoxedValue, ReturnType>(method, nativeObject, outReturn, std::forward<Args>(args)...);
    }

    template <class HypDataType, class ReturnType, class... Args>
    static bool InvokeMethod(const Method* method, const Handle<ObjectBase>& target, ReturnType* outReturn, Args&&... args)
    {
        if (!target || !method)
        {
            return false;
        }

        Array<HypDataType> argsHypData = { HypDataType(target), HypDataType(args)... };

        if constexpr (std::is_void_v<ReturnType>)
        {
            InvokeMethod_Internal(nullptr, method, target, argsHypData);
        }
        else
        {
            HYP_CORE_ASSERT(outReturn != nullptr);

            HypDataType result;
            InvokeMethod_Internal(&result, method, target, argsHypData);

            new (outReturn) ReturnType(result.template Get<ReturnType>());
        }

        return true;
    }
};

/*! \brief A delegate that can be bound to script methods.
 *  \details This delegate can be bound to methods defined in managed code (e.g., C#), HypScript, or native code (reflection methods).
 *  \tparam ReturnType The return type of the delegate.
 *  \tparam Args The argument types of the delegate.*/
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

    HYP_NODISCARD virtual DelegateHandler BindMethod(ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn) override
    {
        if (!getFn)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = ANSIString(methodName), getFn = std::move(getFn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                ValueStorage<ReturnType> returnValueStorage;
                if (!ScriptableDelegateHelper::InvokeScriptObjectMethod<BoxedValue, ReturnType>(getFn(), methodName, returnValueStorage.GetPointer(), std::forward<ArgTypes>(args)...))
                {
                    return ReturnType();
                }

                if constexpr (std::is_void_v<ReturnType>)
                {
                    return;
                }
                else
                {
                    return std::move(returnValueStorage).Get();
                }
            });
    }

    template <class DefaultReturnType, typename = std::enable_if_t<std::is_copy_constructible_v<NormalizedType<DefaultReturnType>>>>
    HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn, DefaultReturnType&& defaultReturn)
    {
        if (!getFn)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = ANSIString(methodName), getFn = std::move(getFn), defaultReturn = std::forward<DefaultReturnType>(defaultReturn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                ScriptObjectResource* scriptObjectResource = getFn();

                if (!scriptObjectResource)
                {
                    return defaultReturn;
                }

                ValueStorage<ReturnType> returnValueStorage;
                if (!ScriptableDelegateHelper::InvokeScriptObjectMethod<BoxedValue, ReturnType>(getFn(), methodName, returnValueStorage.GetPointer(), std::forward<ArgTypes>(args)...))
                {
                    return defaultReturn;
                }

                if constexpr (std::is_void_v<ReturnType>)
                {
                    return;
                }
                else
                {
                    return std::move(returnValueStorage).Get();
                }
            });
    }

    HYP_NODISCARD virtual DelegateHandler BindMethod(ANSIStringView methodName, ScriptObjectResource* scriptObjectResource) override
    {
        if (!scriptObjectResource)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = ANSIString(methodName), scriptObjectResource]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                ValueStorage<ReturnType> returnValueStorage;
                if (!ScriptableDelegateHelper::InvokeScriptObjectMethod<BoxedValue, ReturnType>(scriptObjectResource, methodName, returnValueStorage.GetPointer(), std::forward<ArgTypes>(args)...))
                {
                    return ReturnType();
                }

                if constexpr (std::is_void_v<ReturnType>)
                {
                    return;
                }
                else
                {
                    return std::move(returnValueStorage).Get();
                }
            });
    }

    template <class DefaultReturnType, typename = std::enable_if_t<std::is_copy_constructible_v<NormalizedType<DefaultReturnType>>>>
    HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, ScriptObjectResource* scriptObjectResource, DefaultReturnType&& defaultReturn)
    {
        if (!scriptObjectResource)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = ANSIString(methodName), scriptObjectResource, defaultReturn = std::forward<DefaultReturnType>(defaultReturn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                if (!scriptObjectResource)
                {
                    return defaultReturn;
                }

                ValueStorage<ReturnType> returnValueStorage;
                if (!ScriptableDelegateHelper::InvokeScriptObjectMethod<BoxedValue, ReturnType>(scriptObjectResource, methodName, returnValueStorage.GetPointer(), std::forward<ArgTypes>(args)...))
                {
                    return defaultReturn;
                }

                if constexpr (std::is_void_v<ReturnType>)
                {
                    return;
                }
                else
                {
                    return std::move(returnValueStorage).Get();
                }
            });
    }

#ifdef HYP_DOTNET
    HYP_NODISCARD virtual DelegateHandler BindMethod(ANSIStringView methodName, UniquePtr<dotnet::ManagedObject>&& object) override
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

        if (!object->GetMethod(methodName))
        {
            LogScriptableDelegateError("Failed to find method!", object.Get());

            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = ANSIString(methodName), object = std::move(object)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                return object->InvokeMethodByName<ReturnType>(methodName, std::forward<ArgTypes>(args)...);
            });
    }
#endif

    /*! \brief Call operator overload - alias method for Broadcast().
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref ReturnType if no handlers were bound. */
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes&&... args) const
    {
        return const_cast<ScriptableDelegate*>(this)->Broadcast(std::forward<ArgTypes>(args)...);
    }

private:
};

template <class ReturnType, class... Args>
struct IsDelegate<ScriptableDelegate<ReturnType, Args...>> : std::true_type
{
};

} // namespace functional

using functional::IScriptableDelegate;
using functional::ScriptableDelegate;
using functional::ScriptableDelegateHelper;

} // namespace Hyperion