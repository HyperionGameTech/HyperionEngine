#ifndef HYPERION_LOG_CHANNELS_HPP
#define HYPERION_LOG_CHANNELS_HPP

#include <core/logging/LoggerFwd.hpp>

namespace hyperion {

#pragma region Core

// Parent channel of all following in the region
HYP_DECLARE_LOG_CHANNEL(Core);

HYP_DECLARE_LOG_CHANNEL(Engine);
HYP_DECLARE_LOG_CHANNEL(Config);
HYP_DECLARE_LOG_CHANNEL(Object);
HYP_DECLARE_LOG_CHANNEL(Physics);
HYP_DECLARE_LOG_CHANNEL(Audio);
HYP_DECLARE_LOG_CHANNEL(Input);
HYP_DECLARE_LOG_CHANNEL(Profile);
HYP_DECLARE_LOG_CHANNEL(Math);
HYP_DECLARE_LOG_CHANNEL(DotNET);
HYP_DECLARE_LOG_CHANNEL(Streaming);
HYP_DECLARE_LOG_CHANNEL(UI);

#pragma endregion Core

#pragma region Rendering

HYP_DECLARE_LOG_CHANNEL(Rendering);

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);
HYP_DECLARE_LOG_CHANNEL(RenderCollection);
HYP_DECLARE_LOG_CHANNEL(RenderCommands);
HYP_DECLARE_LOG_CHANNEL(Shader);
HYP_DECLARE_LOG_CHANNEL(Texture);
HYP_DECLARE_LOG_CHANNEL(Mesh);
HYP_DECLARE_LOG_CHANNEL(Material);
HYP_DECLARE_LOG_CHANNEL(Shadows);
HYP_DECLARE_LOG_CHANNEL(EnvProbe);
HYP_DECLARE_LOG_CHANNEL(EnvGrid);
HYP_DECLARE_LOG_CHANNEL(Font);
HYP_DECLARE_LOG_CHANNEL(Lightmap);

#pragma endregion Rendering

#pragma region Net

HYP_DECLARE_LOG_CHANNEL(Net);

HYP_DECLARE_LOG_CHANNEL(Socket);

#pragma endregion Net

#pragma region Threading

HYP_DECLARE_LOG_CHANNEL(Threading);

HYP_DECLARE_LOG_CHANNEL(Tasks);
HYP_DECLARE_LOG_CHANNEL(DataRaceDetector);
HYP_DECLARE_LOG_CHANNEL(Scheduler);

#pragma endregion Threading

#pragma region Scene

HYP_DECLARE_LOG_CHANNEL(Scene);

HYP_DECLARE_LOG_CHANNEL(World);
HYP_DECLARE_LOG_CHANNEL(ECS);
HYP_DECLARE_LOG_CHANNEL(Node);
HYP_DECLARE_LOG_CHANNEL(Camera);
HYP_DECLARE_LOG_CHANNEL(Octree);
HYP_DECLARE_LOG_CHANNEL(Light);
HYP_DECLARE_LOG_CHANNEL(Animation);

#pragma endregion Scene

#pragma region Script

HYP_DECLARE_LOG_CHANNEL(Script);

HYP_DECLARE_LOG_CHANNEL(ScriptingService);

#pragma endregion Script

#pragma region Assets

HYP_DECLARE_LOG_CHANNEL(Assets);

HYP_DECLARE_LOG_CHANNEL(Serialization);

#pragma endregion Assets

} // namespace hyperion

#endif