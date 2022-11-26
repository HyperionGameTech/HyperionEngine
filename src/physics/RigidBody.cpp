#include <physics/RigidBody.hpp>

#include <Engine.hpp>

namespace hyperion::v2::physics {

RigidBody::~RigidBody()
{
    Teardown();
}

void RigidBody::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    // do nothing
}

} // namespace hyperion::v2::physics