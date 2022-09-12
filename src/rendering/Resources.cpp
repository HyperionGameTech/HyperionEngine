#include "Resources.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

Resources::Resources(Engine *engine)
    : m_engine(engine),
      env_probes(engine)
{
}

Resources::~Resources()
{
}

void Resources::Create(Engine *engine)
{
}

void Resources::Destroy(Engine *engine)
{
}

} // namespace hyperion::v2