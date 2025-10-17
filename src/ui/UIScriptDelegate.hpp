/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

#include <scene/Scene.hpp>
#include <scene/components/ScriptComponent.hpp>

#include <core/Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

class UIObject;

#pragma region UIScriptDelegate

enum class UIScriptDelegateFlags : uint32
{
    NONE = 0x0,
    ALLOW_NESTED = 0x1,              //! Allow the method to be called from nested UIObjects.
    REQUIRE_UI_EVENT_ATTRIBUTE = 0x2 //! Require the method to have the Hyperion.UIEvent attribute. Used for default event handlers such as OnClick, OnHover, etc.
};

HYP_MAKE_ENUM_FLAGS(UIScriptDelegateFlags)

template <class... Args>
class UIScriptDelegate
{
public:
    /*! \param uiObject The UIObject to call the method on.
     *  \param methodName The name of the method to call.
     *  \param flags Flags to control the behavior of the delegate.
     */
    UIScriptDelegate(UIObject* uiObject, const String& methodName, EnumFlags<UIScriptDelegateFlags> flags)
        : m_uiObject(uiObject),
          m_methodName(methodName),
          m_flags(flags)
    {
    }

    UIScriptDelegate(const UIScriptDelegate& other) = delete;
    UIScriptDelegate& operator=(const UIScriptDelegate& other) = delete;
    UIScriptDelegate(UIScriptDelegate&& other) noexcept = default;
    UIScriptDelegate& operator=(UIScriptDelegate&& other) noexcept = default;

    virtual ~UIScriptDelegate() = default;

    HYP_FORCE_INLINE UIObject* GetUIObject() const
    {
        return m_uiObject;
    }

    HYP_FORCE_INLINE const String& GetMethodName() const
    {
        return m_methodName;
    }

    UIEventHandlerResult operator()(Args... args)
    {
        Assert(m_uiObject != nullptr);

        const UIEventHandlerResult defaultResult = m_uiObject->GetDefaultEventHandlerResult();

        ScriptComponent* scriptComponent = m_uiObject->GetScriptComponent(bool(m_flags & UIScriptDelegateFlags::ALLOW_NESTED));

        if (!scriptComponent)
        {
            // No script component, do not call.
            return defaultResult;
        }

        if (!scriptComponent->scriptObjectResource || !scriptComponent->scriptObjectResource->GetManagedObject() || !scriptComponent->scriptObjectResource->GetManagedObject()->IsValid())
        {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid ScriptComponent Object"));
        }

        scriptComponent->scriptObjectResource->IncRef();
        HYP_DEFER({ scriptComponent->scriptObjectResource->DecRef(); });

        dotnet::ManagedObject* managedObject = scriptComponent->scriptObjectResource->GetManagedObject();
        Assert(managedObject != nullptr);

        if (dotnet::ManagedClass* classPtr = managedObject->GetClass())
        {
            if (dotnet::Method* methodPtr = classPtr->GetMethod(m_methodName))
            {
                if (m_flags & UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE)
                {
                    if (!methodPtr->GetAttributes().GetAttribute("UIEvent"))
                    {
                        return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Method does not have the Hyperion.UIEvent attribute"));
                    }
                }

                // // Stubbed method, do not call
                // if (methodPtr->GetAttributes().GetAttribute("ScriptMethodStub") != nullptr) {
                //     return defaultResult;
                // }

                UIEventHandlerResult result = managedObject->InvokeMethod<UIEventHandlerResult>(methodPtr, std::forward<Args>(args)...);

                if (result == UIEventHandlerResult::OK)
                {
                    return result | defaultResult;
                }

                return result;
            }

            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Unknown error; method missing on class"));
        }

        HYP_LOG(UI, Error, "Failed to call method {} for UI object with name: {}", m_methodName, m_uiObject->GetName());

        return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Unknown error; failed to call method"));
    }

private:
    UIObject* m_uiObject;
    String m_methodName;
    EnumFlags<UIScriptDelegateFlags> m_flags;
};

#pragma endregion UIScriptDelegate

} // namespace hyperion
