#include "ScriptedController.hpp"

#include <script/ScriptBindings.hpp>
#include <asset/serialization/fbom/marshals/ScriptMarshal.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>

#include <util/UTF8.hpp>

namespace hyperion::v2 {

ScriptedController::ScriptedController()
    : Controller()
{   
}

ScriptedController::ScriptedController(Handle<Script> &&script)
    : Controller()
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

void ScriptedController::Serialize(fbom::FBOMObject &out) const
{
    out.SetProperty("controller_name", fbom::FBOMString(), Memory::StrLen(controller_name), controller_name);

    if (m_script) {
        out.AddChild(*m_script.Get());
    }
}

fbom::FBOMResult ScriptedController::Deserialize(const fbom::FBOMObject &in)
{
    for (auto &sub_object : *in.nodes) {
        if (sub_object.GetType().IsOrExtends("Script")) {
            m_script = sub_object.deserialized.Get<Script>();
        }
    }

    return fbom::FBOMResult::FBOM_OK;
}

} // namespace hyperion::v2