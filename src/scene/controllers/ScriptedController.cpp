#include "ScriptedController.hpp"

#include <script/ScriptBindings.hpp>

#include <util/UTF8.hpp>

namespace hyperion::v2 {

ScriptedController::ScriptedController(Handle<Script> &&script)
    : Controller("ScriptedController")
{
    m_script = std::move(script);
}

void ScriptedController::OnAdded()
{
    Controller::OnAdded();
}

void ScriptedController::OnRemoved()
{
    Controller::OnRemoved();
}

void ScriptedController::OnUpdate(GameCounter::TickUnit delta)
{
    Controller::OnUpdate(delta);
}

} // namespace hyperion::v2