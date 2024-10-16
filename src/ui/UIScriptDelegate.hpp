/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_SCRIPT_DELEGATE_HPP
#define HYPERION_UI_SCRIPT_DELEGATE_HPP

#include <core/containers/Array.hpp>
#include <core/functional/Delegate.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <scene/Scene.hpp>
#include <scene/ecs/components/ScriptComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

class UIObject;

#pragma region UIScriptDelegate

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
     *  \param allow_nested If true, the method can be called from nested UIObjects. */
    UIScriptDelegate(UIObject *ui_object, const String &method_name, bool allow_nested)
        : m_ui_object(ui_object),
          m_method_name(method_name),
          m_allow_nested(allow_nested)
    {
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

        if (!m_ui_object->GetEntity().IsValid()) {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid entity"));
        }

        if (!m_ui_object->GetScene()) {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid scene"));
        }

        ScriptComponent *script_component = m_ui_object->GetScriptComponent(m_allow_nested);

        if (!script_component) {
            // No script component, do not call.
            return UIEventHandlerResult::OK;
        }

        if (!script_component->object) {
            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Invalid ScriptComponent Object"));
        }
        
        if (dotnet::Class *class_ptr = script_component->object->GetClass()) {
            if (dotnet::ManagedMethod *method_ptr = class_ptr->GetMethod(m_method_name)) {
                if (!method_ptr->HasAttribute("Hyperion.UIEvent")) {
                    return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Method does not have the Hyperion.UIEvent attribute"));
                }

                // Stubbed method, do not call
                if (method_ptr->HasAttribute("Hyperion.ScriptMethodStub")) {
                    // // Stubbed method, do not bother calling it
                    HYP_LOG(UI, LogLevel::INFO, "Stubbed method {} for UI object with name: {}", m_method_name, m_ui_object->GetName());

                    return UIEventHandlerResult::OK;
                }

                return script_component->object->InvokeMethod<UIEventHandlerResult>(method_ptr);
            }

            return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Unknown error; method missing on class"));
        }

        HYP_LOG(UI, LogLevel::ERR, "Failed to call method {} for UI object with name: {}", m_method_name, m_ui_object->GetName());

        return UIEventHandlerResult(UIEventHandlerResult::ERR, HYP_STATIC_MESSAGE("Unknown error; failed to call method"));
    }
    
private:
    UIObject    *m_ui_object;
    String      m_method_name;
    bool        m_allow_nested;
};

#pragma endregion UIScriptDelegate

} // namespace hyperion

#endif