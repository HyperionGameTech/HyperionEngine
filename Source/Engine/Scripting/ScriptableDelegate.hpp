/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scripting/ScriptableDelegateFwd.hpp>
#include <Scripting/ScriptObjectResource.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Resource/Resource.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Threading/SharedMutex.hpp>

#ifdef HYP_DOTNET
#include <DotNET/ManagedObject.hpp>
#endif

namespace Hyperion {

class Method;
class ObjectBase;
struct BoxedValue;

extern "C" Method* Class_GetMethod(const Class* cls, const Name* methodName);

namespace functional {

ENGINE_API void LogScriptableDelegateError(const char* message, dotnet::ManagedObject* objectPtr);

class IScriptableDelegate
{
public:
    virtual ~IScriptableDelegate() = default;

    virtual HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn) = 0;
    virtual HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, ScriptObjectResource* scriptObjectResource) = 0;

    virtual HYP_NODISCARD DelegateHandler BindMethod(void* target, ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn) = 0;
    virtual HYP_NODISCARD DelegateHandler BindMethod(void* target, ANSIStringView methodName, ScriptObjectResource* scriptObjectResource) = 0;

#ifdef HYP_DOTNET
    virtual HYP_NODISCARD DelegateHandler BindMethod(ANSIStringView methodName, UniquePtr<dotnet::ManagedObject>&& object) = 0;
    virtual HYP_NODISCARD DelegateHandler BindMethod(void* target, ANSIStringView methodName, UniquePtr<dotnet::ManagedObject>&& object) = 0;
#endif

    virtual int RemoveAllDetached() = 0;

    virtual int RemoveAllForTarget(void* target) = 0;

    virtual bool Remove(DelegateHandler&& handle) = 0;

    virtual int RemoveAllFromSet(DelegateHandlerSet& handlerSet) const = 0;
};

class ScriptableDelegateHelper
{
public:
    static void InvokeMethod_Internal(BoxedValue* pOutBoxed, const Method* method, const Handle<ObjectBase>& target, Span<BoxedValue> argsBoxed);

    template <class TBoxed, class ReturnType, class... Args>
    static bool InvokeScriptObjectMethod(ScriptObjectResource* scriptObjectResource, ANSIStringView methodName, ReturnType* outReturn, Args&&... args)
    {
        HYP_CORE_ASSERT(scriptObjectResource != nullptr, "Script object resource is null!");

        scriptObjectResource->AddReader();
        HYP_DEFER({ scriptObjectResource->ReleaseReader(); });

#ifdef HYP_DOTNET
        if (scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)))
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
        if (scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::HypScript)))
        {
            ScriptObjectData_HypScript* hypScriptData = scriptObjectResource->GetScriptObjectData_HypScript();
            HYP_CORE_ASSERT(hypScriptData != nullptr, "HypScript script object data is null!");

            ScriptInstance* instance = hypScriptData->instance;
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

        const Name name = Name(StringHash(methodName));

        const Method* method = Class_GetMethod(nativeObject->InstanceClass(), &name);
        if (!method)
        {
            return false;
        }

        return InvokeMethod<BoxedValue, ReturnType>(method, nativeObject, outReturn, std::forward<Args>(args)...);
    }

    template <class TBoxed, class ReturnType, class... Args>
    static bool InvokeMethod(const Method* method, const Handle<ObjectBase>& target, ReturnType* outReturn, Args&&... args)
    {
        if (!target || !method)
        {
            return false;
        }

        Array<TBoxed> argsBoxed = { TBoxed(target), TBoxed(args)... };

        if constexpr (std::is_void_v<ReturnType>)
        {
            InvokeMethod_Internal(nullptr, method, target, argsBoxed);
        }
        else
        {
            HYP_CORE_ASSERT(outReturn != nullptr);

            TBoxed result;
            InvokeMethod_Internal(&result, method, target, argsBoxed);

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
class ScriptableDelegate final : public IScriptableDelegate
{
public:
    using ProcType = Proc<ReturnType(Args...)>;

    ScriptableDelegate() = default;
    ScriptableDelegate(const ScriptableDelegate& other) = delete;
    ScriptableDelegate& operator=(const ScriptableDelegate& other) = delete;

    ScriptableDelegate(ScriptableDelegate&& other) noexcept = default;
    ScriptableDelegate& operator=(ScriptableDelegate&& other) noexcept = delete;

    virtual ~ScriptableDelegate() override = default;

    /*! Non-target overloads — delegate to target-aware versions with nullptr target. */
    HYP_NODISCARD virtual DelegateHandler BindMethod(ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn) override
    {
        return BindMethod(nullptr, methodName, std::move(getFn));
    }

    HYP_NODISCARD virtual DelegateHandler BindMethod(ANSIStringView methodName, ScriptObjectResource* scriptObjectResource) override
    {
        return BindMethod(nullptr, methodName, scriptObjectResource);
    }

#ifdef HYP_DOTNET
    HYP_NODISCARD virtual DelegateHandler BindMethod(ANSIStringView methodName, UniquePtr<dotnet::ManagedObject>&& object) override
    {
        return BindMethod(nullptr, methodName, std::move(object));
    }
#endif

    /*! binds a handler scoped to a specific target */
    HYP_NODISCARD virtual DelegateHandler BindMethod(void* target, ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn) override
    {
        if (!getFn)
        {
            return DelegateHandler();
        }

        return GetOrCreatePerTargetDelegate(target)->Bind([methodName = ANSIString(methodName), getFn = std::move(getFn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
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

    HYP_NODISCARD virtual DelegateHandler BindMethod(void* target, ANSIStringView methodName, ScriptObjectResource* scriptObjectResource) override
    {
        if (!scriptObjectResource)
        {
            return DelegateHandler();
        }

        return GetOrCreatePerTargetDelegate(target)->Bind([methodName = ANSIString(methodName), scriptObjectResource]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
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
    HYP_NODISCARD DelegateHandler BindMethod(void* target, ANSIStringView methodName, Proc<ScriptObjectResource*()>&& getFn, DefaultReturnType&& defaultReturn)
    {
        if (!getFn)
        {
            return DelegateHandler();
        }

        return GetOrCreatePerTargetDelegate(target)->Bind([methodName = ANSIString(methodName), getFn = std::move(getFn), defaultReturn = std::forward<DefaultReturnType>(defaultReturn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
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

    template <class DefaultReturnType, typename = std::enable_if_t<std::is_copy_constructible_v<NormalizedType<DefaultReturnType>>>>
    HYP_NODISCARD DelegateHandler BindMethod(void* target, ANSIStringView methodName, ScriptObjectResource* scriptObjectResource, DefaultReturnType&& defaultReturn)
    {
        if (!scriptObjectResource)
        {
            return DelegateHandler();
        }

        return GetOrCreatePerTargetDelegate(target)->Bind([methodName = ANSIString(methodName), scriptObjectResource, defaultReturn = std::forward<DefaultReturnType>(defaultReturn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
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
    HYP_NODISCARD virtual DelegateHandler BindMethod(void* target, ANSIStringView methodName, UniquePtr<dotnet::ManagedObject>&& object) override
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

        return GetOrCreatePerTargetDelegate(target)->Bind([methodName = ANSIString(methodName), object = std::move(object)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                return object->InvokeMethodByName<ReturnType>(methodName, std::forward<ArgTypes>(args)...);
            });
    }
#endif // HYP_DOTNET

    virtual int RemoveAllDetached() override
    {
        int count = 0;

        {
            TUniqueLock guard(m_perTargetDelegatesMutex);
            for (auto& pair : m_perTargetDelegates)
            {
                count += pair.second->RemoveAllDetached();
            }
        }

        return count;
    }

    virtual int RemoveAllForTarget(void* target) override
    {
        TUniqueLock guard(m_perTargetDelegatesMutex);

        auto it = m_perTargetDelegates.Find(target);
        if (it == m_perTargetDelegates.End())
        {
            return 0;
        }

        const int count = it->second->RemoveAllDetached();
        m_perTargetDelegates.Erase(it);

        return count;
    }

    virtual bool Remove(DelegateHandler&& handle) override
    {
        if (!handle.IsValid())
        {
            return false;
        }
        
        SharedPtr<void> strongRef = handle.delegateImpl.Lock();
        if (strongRef != nullptr)
        {
            handle.removeFn(strongRef.Get(), handle.entry);
        }

        handle.entry = nullptr;

        return true;
    }

    virtual int RemoveAllFromSet(DelegateHandlerSet& handlerSet) const override
    {
        int count = 0;

        TSharedLock guard(m_perTargetDelegatesMutex);
        for (auto& pair : m_perTargetDelegates)
        {
            count += handlerSet.Remove(pair.second.Get());
        }

        return count;
    }

    HYP_NODISCARD DelegateHandler Bind(void* target, Proc<ReturnType(Args...)>&& proc)
    {
        return GetOrCreatePerTargetDelegate(target)->Bind(std::move(proc));
    }

    HYP_NODISCARD DelegateHandler BindThreaded(void* target, Proc<ReturnType(Args...)>&& proc, const ThreadId& callingThreadId)
    {
        return GetOrCreatePerTargetDelegate(target)->BindThreaded(std::move(proc), callingThreadId);
    }

    template <class... ArgTypes>
    ReturnType Fire(void* target, ArgTypes&&... args)
    {
        SharedPtr<Delegate<ReturnType, Args...>> perTargetDelegate = FindPerTargetDelegate(target);

        if (!perTargetDelegate)
        {
            return ReturnType();
        }

        return perTargetDelegate->Broadcast(std::forward<ArgTypes>(args)...);
    }
    
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes&&... args) const
    {
        return const_cast<ScriptableDelegate*>(this)->BroadcastAll(std::forward<ArgTypes>(args)...);
    }

private:
    SharedPtr<Delegate<ReturnType, Args...>> GetOrCreatePerTargetDelegate(void* target)
    {
        TUniqueLock guard(m_perTargetDelegatesMutex);

        auto it = m_perTargetDelegates.Find(target);
        if (it == m_perTargetDelegates.End())
        {
            it = m_perTargetDelegates.Insert({ target, MakeShared<Delegate<ReturnType, Args...>>() }).first;
        }

        return it->second;
    }

    HYP_FORCE_INLINE SharedPtr<Delegate<ReturnType, Args...>> FindPerTargetDelegate(void* target) const
    {
        TSharedLock guard(m_perTargetDelegatesMutex);

        auto it = m_perTargetDelegates.Find(target);
        if (it == m_perTargetDelegates.End())
        {
            return nullptr;
        }

        return it->second;
    }

    template <class... ArgTypes>
    ReturnType BroadcastAll(ArgTypes&&... args)
    {
        if constexpr (std::is_same_v<ReturnType, void>)
        {
            TSharedLock guard(m_perTargetDelegatesMutex);
            for (auto& pair : m_perTargetDelegates)
            {
                pair.second->Broadcast(std::forward<ArgTypes>(args)...);
            }
        }
        else
        {
            ReturnType result = ReturnType();

            TSharedLock guard(m_perTargetDelegatesMutex);
            for (auto& pair : m_perTargetDelegates)
            {
                result = pair.second->Broadcast(std::forward<ArgTypes>(args)...);
            }

            return result;
        }
    }

    mutable SharedMutex m_perTargetDelegatesMutex;
    FlatMap<void*, SharedPtr<Delegate<ReturnType, Args...>>> m_perTargetDelegates;
};

template <class ReturnType, class... Args>
struct IsDelegate<ScriptableDelegate<ReturnType, Args...>> : std::true_type
{
    // Temporary:
    static_assert(!std::is_base_of_v<Delegate<ReturnType, Args...>, ScriptableDelegate<ReturnType, Args...>>,
        "ScriptableDelegate no longer inherits from Delegate, please verify code correctness matches with new ScriptableDelegate API");
};

} // namespace functional

using functional::IScriptableDelegate;
using functional::ScriptableDelegate;
using functional::ScriptableDelegateHelper;

} // namespace Hyperion
