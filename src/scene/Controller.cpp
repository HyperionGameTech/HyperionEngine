#include "Controller.hpp"
#include <scene/Entity.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

Controller::Controller(Bool receives_update)
    : m_owner(nullptr),
      m_receives_update(receives_update)
{
}

Controller::~Controller()
{
}

void Controller::OnAdded()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void Controller::OnRemoved()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void Controller::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);
}

} // namespace hyperion::v2