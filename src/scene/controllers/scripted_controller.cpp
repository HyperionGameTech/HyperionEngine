#include "scripted_controller.h"

#include <script/ScriptBindings.hpp>

#include <util/utf8.hpp>

namespace hyperion::v2 {

ScriptedController::ScriptedController(std::unique_ptr<Script> &&script)
    : Controller("ScriptedController"),
      m_script(std::move(script))
{
}

void ScriptedController::OnAdded()
{
    if (m_script == nullptr) {
        return;
    }

    APIInstance api_instance;
    ScriptBindings::Build(api_instance);

    if (m_script->Compile(api_instance)) {
        m_script->Bake();
        m_script->Decompile(&utf::cout);
        
        m_script->Run();

        m_script->CallFunction(init_function_name, GetParent());
    } else {
        DebugLog(LogType::Error, "Script compilation failed!\n");

        m_script->GetErrors().WriteOutput(utf::cout);

        HYP_BREAKPOINT;
    }
}

void ScriptedController::OnRemoved()
{
    m_script->CallFunction(removed_function_name, GetParent());
}

void ScriptedController::OnUpdate(GameCounter::TickUnit delta)
{
    m_script->CallFunction(tick_function_name, GetParent(), delta);
}

} // namespace hyperion::v2