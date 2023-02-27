#include "Controller.hpp"
#include <scene/Entity.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

Controller::Controller(bool receives_update)
    : m_owner(nullptr),
      m_receives_update(receives_update),
      m_script_valid(false)
{
}

Controller::~Controller()
{
}

void Controller::SetScript(const Handle<Script> &script)
{
    m_script = script;
    m_script_valid = false;
    m_script_methods = { };
    m_self_object = { };

    if (m_script != nullptr) {
        if (GetOwner() != nullptr) {
            if ((m_script_valid = CreateScriptedObjects())) {
                m_script->CallFunction(
                    m_script_methods[SCRIPT_METHOD_ON_ADDED],
                    m_self_object,
                    m_script->CreateInternedObject<Handle<Entity>>(Handle<Entity>(GetOwner()->GetID()))
                );
            }
        }
    }
}

void Controller::SetScript(Handle<Script> &&script)
{
    m_script = std::move(script);
    m_script_valid = false;
    m_script_methods = { };
    m_self_object = { };

    if (m_script != nullptr) {
        if (GetOwner() != nullptr) {
            if ((m_script_valid = CreateScriptedObjects())) {
                m_script->CallFunction(
                    m_script_methods[SCRIPT_METHOD_ON_ADDED],
                    m_self_object,
                    m_script->CreateInternedObject<Handle<Entity>>(Handle<Entity>(GetOwner()->GetID()))
                );
            }
        }
    }
}

bool Controller::CreateScriptedObjects()
{
    if (!m_script) {
        return false;
    }

    if (m_script->Compile()) {
        m_script->Bake();
        // m_script->Decompile(&utf::cout);
        m_script->Run();

        Script::ObjectHandle engine_object_handle;

        if (!m_script->GetObjectHandle("_engine", engine_object_handle)) {
            DebugLog(
                LogType::Error,
                "Failed to get `_engine` object\n"
            );

            return false;
        }

        vm::Value engine_handle_value(vm::Value::ValueType::USER_DATA, { .user_data = static_cast<void *>(GetEngine()) });

        if (!m_script->SetMember(engine_object_handle, "handle", engine_handle_value)) {
            DebugLog(
                LogType::Error,
                "Failed to set `_engine.handle` member\n"
            );

            return false;
        }

        if (!m_script->GetObjectHandle("controller", m_self_object)) {
            DebugLog(
                LogType::Error,
                "Failed to get `controller` object\n"
            );

            return false;
        }

        Script::ValueHandle receives_update_handle;

        if (m_script->GetMember(m_self_object, "receives_update", receives_update_handle)) {
            receives_update_handle._inner.GetBoolean(&m_receives_update);
        } else {
            m_receives_update = false;
        }

        if (!CreateScriptedMethods()) {
            return false;
        }

        return true;
    } else {
        DebugLog(LogType::Error, "Script compilation failed!\n");

        m_script->GetErrors().WriteOutput(std::cout);

        return false;
    }
}

bool Controller::CreateScriptedMethods()
{
    if (!m_script->GetMember(m_self_object, "OnAdded", m_script_methods[SCRIPT_METHOD_ON_ADDED])) {
        DebugLog(
            LogType::Error,
            "Failed to get `OnAdded` method\n"
        );

        return false;
    }

    if (!m_script->GetMember(m_self_object, "OnTick", m_script_methods[SCRIPT_METHOD_ON_TICK])) {
        DebugLog(
            LogType::Error,
            "Failed to get `OnTick` method\n"
        );

        return false;
    }

    if (!m_script->GetMember(m_self_object, "OnRemoved", m_script_methods[SCRIPT_METHOD_ON_REMOVED])) {
        DebugLog(
            LogType::Error,
            "Failed to get `OnRemoved` method\n"
        );

        return false;
    }

    return true;
}

void Controller::OnAdded()
{
    Threads::AssertOnThread(THREAD_GAME);

    if (HasScript()) {
        if ((m_script_valid = CreateScriptedObjects())) {
            m_script->CallFunction(
                m_script_methods[SCRIPT_METHOD_ON_ADDED],
                m_self_object,
                m_script->CreateInternedObject<Handle<Entity>>(Handle<Entity>(GetOwner()->GetID()))
            );
        }
    }
}

void Controller::OnRemoved()
{
    Threads::AssertOnThread(THREAD_GAME);

    if (HasScript() && IsScriptValid()) {
        m_script->CallFunction(
            m_script_methods[SCRIPT_METHOD_ON_REMOVED],
            m_self_object,
            m_script->CreateInternedObject<Handle<Entity>>(Handle<Entity>(GetOwner()->GetID()))
        );
    }
}

void Controller::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (HasScript() && IsScriptValid()) {
        m_script->CallFunction(m_script_methods[SCRIPT_METHOD_ON_TICK], m_self_object, Float(delta));
    }
}

} // namespace hyperion::v2