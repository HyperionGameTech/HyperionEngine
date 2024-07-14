#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

#pragma region Core

HYP_DEFINE_LOG_CHANNEL(Core);

HYP_DEFINE_LOG_SUBCHANNEL(Engine, Core);
HYP_DEFINE_LOG_SUBCHANNEL(Config, Core);
HYP_DEFINE_LOG_SUBCHANNEL(Physics, Core);
HYP_DEFINE_LOG_SUBCHANNEL(Audio, Core);
HYP_DEFINE_LOG_SUBCHANNEL(Input, Core);
HYP_DEFINE_LOG_SUBCHANNEL(Profile, Core);
HYP_DEFINE_LOG_SUBCHANNEL(Math, Core);
HYP_DEFINE_LOG_SUBCHANNEL(DotNET, Core);
HYP_DEFINE_LOG_SUBCHANNEL(Streaming, Core);
HYP_DEFINE_LOG_SUBCHANNEL(UI, Core);

#pragma endregion Core

#pragma region Rendering

HYP_DEFINE_LOG_SUBCHANNEL(Rendering, Core);

HYP_DEFINE_LOG_SUBCHANNEL(RenderingBackend, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(RenderCollection, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(RenderCommands, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(Shader, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(Texture, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(Mesh, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(Material, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(Shadows, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(EnvProbe, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(EnvGrid, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(Font, Rendering);
HYP_DEFINE_LOG_SUBCHANNEL(Lightmap, Rendering);

#pragma endregion Rendering

#pragma region Net

HYP_DEFINE_LOG_SUBCHANNEL(Net, Core);

HYP_DEFINE_LOG_SUBCHANNEL(Socket, Net);

#pragma endregion Net

#pragma region Threading

HYP_DEFINE_LOG_SUBCHANNEL(Threading, Core);

HYP_DEFINE_LOG_SUBCHANNEL(Tasks, Threading);

#pragma endregion Threading

#pragma region Scene

HYP_DEFINE_LOG_SUBCHANNEL(Scene, Core);

HYP_DEFINE_LOG_SUBCHANNEL(World, Scene);
HYP_DEFINE_LOG_SUBCHANNEL(ECS, Scene);
HYP_DEFINE_LOG_SUBCHANNEL(Node, Scene);
HYP_DEFINE_LOG_SUBCHANNEL(Camera, Scene);
HYP_DEFINE_LOG_SUBCHANNEL(Octree, Scene);
HYP_DEFINE_LOG_SUBCHANNEL(Light, Scene);

#pragma endregion Scene

#pragma region Script

HYP_DEFINE_LOG_SUBCHANNEL(Script, Core);

HYP_DEFINE_LOG_SUBCHANNEL(ScriptingService, Script);

#pragma endregion Script

#pragma region Assets

HYP_DEFINE_LOG_SUBCHANNEL(Assets, Core);

HYP_DEFINE_LOG_SUBCHANNEL(Serialization, Assets);

#pragma endregion Assets

} // namespace hyperion