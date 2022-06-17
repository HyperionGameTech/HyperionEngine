#include "controller.h"

namespace hyperion::v2 {

std::atomic<ControllerId> ControllerSet::controller_id_counter{0};

Controller::Controller(const char *name)
    : m_name(nullptr),
      m_parent(nullptr)
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

} // namespace hyperion::v2