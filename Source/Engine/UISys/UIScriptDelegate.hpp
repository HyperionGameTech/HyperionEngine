/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Array.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/DeferredScope.hpp>

#include <scene/Scene.hpp>
#include <scene/components/ScriptComponent.hpp>

#include <Core/Types.hpp>

#ifdef HYP_DOTNET
#include <dotnet/ManagedClass.hpp>
#endif

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(UI);

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
    UIScriptDelegate(UIObject* uiObject, const ANSIString& methodName, EnumFlags<UIScriptDelegateFlags> flags)
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

    HYP_FORCE_INLINE const ANSIString& GetMethodName() const
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

        if (!scriptComponent->scriptObjectResource)
        {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid ScriptComponent Object"));
        }

        scriptComponent->scriptObjectResource->AddReader();
        HYP_DEFER({ scriptComponent->scriptObjectResource->ReleaseReader(); });

#ifdef HYP_DOTNET
        if (scriptComponent->scriptObjectResource->GetScriptLanguageMask() & (1u << uint32(ScriptLanguage::CSharp)))
        {
            dotnet::ManagedObject* managedObject = scriptComponent->scriptObjectResource->GetManagedObject();
            Assert(managedObject != nullptr);

            if (dotnet::ManagedClass* managedClass = managedObject->GetClass())
            {
                if (dotnet::ManagedMethod* managedMethod = managedClass->GetMethod(m_methodName))
                {
                    if (m_flags & UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE)
                    {
                        if (!managedMethod->GetAttributes().GetAttribute("UIEvent"))
                        {
                            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Method does not have the Hyperion.UIEvent attribute"));
                        }
                    }

                    // // Stubbed method, do not call
                    // if (managedMethod->GetAttributes().GetAttribute("ScriptMethodStub") != nullptr) {
                    //     return defaultResult;
                    // }

                    UIEventHandlerResult result = managedObject->InvokeMethod<UIEventHandlerResult>(managedMethod, std::forward<Args>(args)...);

                    if (result == UIEventHandlerResult::OK)
                    {
                        return result | defaultResult;
                    }

                    return result;
                }

                return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Unknown error; method missing on class"));
            }
        }
#endif

        UIEventHandlerResult result;

        if (!ScriptableDelegateHelper::InvokeScriptObjectMethod<BoxedValue, UIEventHandlerResult>(
                scriptComponent->scriptObjectResource,
                m_methodName,
                &result,
                std::forward<Args>(args)...))
        {
            HYP_LOG(UI, Error, "Failed to call method {} for UI object with name: {}", m_methodName, m_uiObject->GetName());
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Failed to call method"));
        }

        return result;
    }

private:
    UIObject* m_uiObject;
    ANSIString m_methodName;
    EnumFlags<UIScriptDelegateFlags> m_flags;
};

#pragma endregion UIScriptDelegate

} // namespace Hyperion
