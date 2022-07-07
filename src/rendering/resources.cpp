#include "Resources.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

Resources::Resources(Engine *engine)
    : shaders(engine->callbacks,            {engine}),
      textures(engine->callbacks,           {engine}),
      materials(engine->callbacks,          {engine}),
      lights(engine->callbacks,             {engine}),
      spatials(engine->callbacks,           {engine}),
      meshes(engine->callbacks,             {engine}),
      skeletons(engine->callbacks,          {engine}),
      scenes(engine->callbacks,             {engine}),
      render_passes(engine->callbacks,      {engine}),
      framebuffers(engine->callbacks,       {engine}),
      graphics_pipelines(engine->callbacks, {engine}),
      compute_pipelines(engine->callbacks,  {engine}),
      blas(engine->callbacks,               {engine})
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