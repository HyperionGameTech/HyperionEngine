#ifndef HYPERION_V2_SCRIPTED_CONTROLLER_H
#define HYPERION_V2_SCRIPTED_CONTROLLER_H

#include "../controller.h"

#include <script/Script.hpp>

#include <memory>

namespace hyperion::v2 {

class ScriptedController : public Controller {
    static constexpr const char *init_function_name    = "OnAdded";
    static constexpr const char *removed_function_name = "OnRemoved";
    static constexpr const char *tick_function_name    = "OnTick";

public:
    ScriptedController(std::unique_ptr<Script> &&script);
    virtual ~ScriptedController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    std::unique_ptr<Script> m_script;

    Value                   m_node_script_value;
};

} // namespace hyperion::v2

#endif
