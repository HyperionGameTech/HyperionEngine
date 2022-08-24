#include "Resources.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

Resources::Resources(Engine *engine)
    : m_engine(engine),
      entities(engine),
      render_passes(engine),
      framebuffers(engine),
      blas(engine),
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
    // framebuffers.Clear();
    // renderer_instances.Clear();
    // render_passes.Clear();
    // // tlas.Clear();
    // blas.Clear();
    // env_probes.Clear();
    // entities.Clear();
}

} // namespace hyperion::v2