#include "ScriptedController.hpp"

#include <script/ScriptBindings.hpp>

#include <util/Utf8.hpp>

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

    if (m_script->Compile(api_instance)) {
        m_script->Bake();
        m_script->Decompile(&utf::cout);
        
        m_script->Run();

        m_script->CallFunction(init_function_name, GetOwner());
    } else {
        DebugLog(LogType::Error, "Script compilation failed!\n");

        m_script->GetErrors().WriteOutput(std::cout);

        HYP_BREAKPOINT;
    }
}

void ScriptedController::OnRemoved()
{
    m_script->CallFunction(removed_function_name, GetOwner());
}

void ScriptedController::OnUpdate(GameCounter::TickUnit delta)
{
    m_script->CallFunction(tick_function_name, GetOwner(), delta);
}

} // namespace hyperion::v2