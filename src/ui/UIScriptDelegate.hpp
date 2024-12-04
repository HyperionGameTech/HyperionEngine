/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_SCRIPT_DELEGATE_HPP
#define HYPERION_UI_SCRIPT_DELEGATE_HPP

#include <core/containers/Array.hpp>

#include <core/functional/Delegate.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/logging/Logger.hpp>

#include <scene/Scene.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

class UIObject;

#pragma region UIScriptDelegate

enum class UIScriptDelegateFlags : uint32
{
    NONE                        = 0x0,
    ALLOW_NESTED                = 0x1,  //! Allow the method to be called from nested UIObjects.
    REQUIRE_UI_EVENT_ATTRIBUTE  = 0x2   //! Require the method to have the Hyperion.UIEvent attribute. Used for default event handlers such as OnClick, OnHover, etc.
};

HYP_MAKE_ENUM_FLAGS(UIScriptDelegateFlags)

class IUIScriptDelegate
{
public:
    virtual ~IUIScriptDelegate() = default;

    virtual UIObject *GetUIObject() const = 0;
    virtual const String &GetMethodName() const = 0;
};

template <class... Args>
class UIScriptDelegate : public IUIScriptDelegate
{
public:
    /*! \param ui_object The UIObject to call the method on.
     *  \param method_name The name of the method to call.
     *  \param flags Flags to control the behavior of the delegate.
     */
    UIScriptDelegate(UIObject *ui_object, const String &method_name, EnumFlags<UIScriptDelegateFlags> flags)
        : m_ui_object(ui_object),
          m_method_name(method_name),
          m_flags(flags)
    {
        HYP_LOG(UI, LogLevel::DEBUG, "Creating UIScriptDelegate for method {} on UI object with name: {}", m_method_name, m_ui_object->GetName());
    }

    UIScriptDelegate(const UIScriptDelegate &other)                 = delete;
    UIScriptDelegate &operator=(const UIScriptDelegate &other)      = delete;
    UIScriptDelegate(UIScriptDelegate &&other) noexcept             = default;
    UIScriptDelegate &operator=(UIScriptDelegate &&other) noexcept  = default;

    virtual ~UIScriptDelegate() override                            = default;

    virtual UIObject *GetUIObject() const override
    {
        return m_ui_object;
    }

    virtual const String &GetMethodName() const override
    {
        return m_method_name;
    }

    UIEventHandlerResult operator()(Args... args)
    {
        AssertThrow(m_ui_object != nullptr);

        HYP_LOG(UI, LogLevel::DEBUG, "Attempting to call method {} for UI object with name: {}", m_method_name, m_ui_object->GetName());

        const UIEventHandlerResult default_result = m_ui_object->GetDefaultEventHandlerResult();

        if (!m_ui_object->GetEntity().IsValid()) {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid entity"));
        }

        if (!m_ui_object->GetScene()) {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid scene"));
        }

        ScriptComponent *script_component = m_ui_object->GetScriptComponent(bool(m_flags & UIScriptDelegateFlags::ALLOW_NESTED));

        if (!script_component) {
            // No script component, do not call.
            return default_result;
        }

        if (!script_component->object) {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid ScriptComponent Object"));
        }
        
        if (dotnet::Class *class_ptr = script_component->object->GetClass()) {
            if (dotnet::Method *method_ptr = class_ptr->GetMethod(m_method_name)) {
                if (m_flags & UIScriptDelegateFlags::REQUIRE_UI_EVENT_ATTRIBUTE) {
                    if (!method_ptr->GetAttributes().GetAttribute("UIEvent")) {
                        return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Method does not have the Hyperion.UIEvent attribute"));
                    }
                }

                // Stubbed method, do not call
                if (method_ptr->GetAttributes().GetAttribute("ScriptMethodStub") != nullptr) {
                    return default_result;
                }

                UIEventHandlerResult result = script_component->object->InvokeMethod<UIEventHandlerResult>(method_ptr, std::forward<Args>(args)...);

                if (result == UIEventHandlerResult::OK) {
                    return result | default_result;
                }

                return result;
            }

            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Unknown error; method missing on class"));
        }

        HYP_LOG(UI, LogLevel::ERR, "Failed to call method {} for UI object with name: {}", m_method_name, m_ui_object->GetName());

        return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Unknown error; failed to call method"));
    }
    
private:
    UIObject                            *m_ui_object;
    String                              m_method_name;
    EnumFlags<UIScriptDelegateFlags>    m_flags;
};

#pragma endregion UIScriptDelegate

} // namespace hyperion

#endif