#ifndef HYPERION_V2_SCRIPTED_CONTROLLER_H
#define HYPERION_V2_SCRIPTED_CONTROLLER_H

#include <scene/Controller.hpp>
#include <script/Script.hpp>
#include <core/Handle.hpp>

namespace hyperion::v2 {

class ScriptedController : public Controller
{
public:
    ScriptedController(Handle<Script> &&script);
    virtual ~ScriptedController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif
