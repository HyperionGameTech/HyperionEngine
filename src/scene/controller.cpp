#include "Controller.hpp"
#include <scene/Spatial.hpp>

namespace hyperion::v2 {

std::atomic<ControllerId> ControllerSet::controller_id_counter{0};

Controller::Controller(const char *name, bool receives_update)
    : m_name(nullptr),
      m_owner(nullptr),
      m_receives_update(receives_update)
{
    if (name != nullptr) {
        size_t len = std::strlen(name);

        m_name = new char[len + 1];
        std::strcpy(m_name, name);
    }
}

Controller::~Controller()
{
    if (m_name != nullptr) {
        delete[] m_name;
    }
}

Engine *Controller::GetEngine() const
{
    AssertThrow(m_owner != nullptr);

    return m_owner->GetEngine();
}

} // namespace hyperion::v2