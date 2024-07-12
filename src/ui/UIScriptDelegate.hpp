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

template <class... Args>
struct UIScriptDelegate
{
    UIObject    *ui_object;
    String      method_name;

    UIScriptDelegate(UIObject *ui_object, const String &method_name)
        : ui_object(ui_object),
          method_name(method_name)
    {
    }

    UIEventHandlerResult operator()(Args... args)
    {
        AssertThrow(ui_object != nullptr);

        if (!ui_object->GetEntity().IsValid()) {
            return UIEventHandlerResult::ERR;
        }

        if (!ui_object->GetScene()) {
            return UIEventHandlerResult::ERR;
        }

        ScriptComponent *script_component = ui_object->GetScene()->GetEntityManager()->TryGetComponent<ScriptComponent>(ui_object->GetEntity());

        if (!script_component) {
            // No script component, do not call.
            return UIEventHandlerResult::OK;
        }

        if (!script_component->object) {
            return UIEventHandlerResult::ERR;
        }
        
        if (dotnet::Class *class_ptr = script_component->object->GetClass()) {
            if (dotnet::ManagedMethod *method_ptr = class_ptr->GetMethod(method_name)) {
                // Stubbed method, do not call
                if (method_ptr->HasAttribute("Hyperion.ScriptMethodStub")) {
                    // // Stubbed method, do not bother calling it
                    HYP_LOG(UI, LogLevel::INFO, "Stubbed method {} for UI object with name: {}", method_name, ui_object->GetName());

                    return UIEventHandlerResult::OK;
                }

                return script_component->object->InvokeMethod<UIEventHandlerResult>(method_ptr);
            }
        }

        HYP_LOG(UI, LogLevel::ERR, "Failed to call method {} for UI object with name: {}", method_name, ui_object->GetName());

        return UIEventHandlerResult::ERR;
    }
};

#pragma endregion UIScriptDelegate

} // namespace hyperion

#endif