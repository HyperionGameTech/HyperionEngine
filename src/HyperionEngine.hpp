#ifndef HYPERION_ENGINE_HPP
#define HYPERION_ENGINE_HPP

#include <Engine.hpp>
#include <system/Application.hpp>

namespace hyperion {

using namespace v2;

void InitializeApplication(RC<Application> application);
void ShutdownApplication();

} // namespace hyperion

#endif