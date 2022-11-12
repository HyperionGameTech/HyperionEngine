#ifndef HYPERION_V2_SCRIPTED_CONTROLLER_H
#define HYPERION_V2_SCRIPTED_CONTROLLER_H

#include <scene/Controller.hpp>
#include <script/Script.hpp>
#include <core/Handle.hpp>

namespace hyperion::v2 {

class ScriptedController : public Controller
{
    Script::ObjectHandle m_controller_object;
    Script::FunctionHandle m_onadded;
    Script::FunctionHandle m_onremoved;
    Script::FunctionHandle m_ontick;

public:
    ScriptedController(Handle<Script> &&script);
    virtual ~ScriptedController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    Handle<Script> m_script;

    Value m_node_script_value;
};

} // namespace hyperion::v2

#endif
