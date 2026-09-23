#pragma once

#include <Editor/EditorCommand.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorViewport.hpp>

#include <Editor/Tasks/EditorTasks.hpp>

#include <Editor/Terrain/EditorTerrainState.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>
#include <Scene/Light/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/InstancedMeshProxy.hpp>
#include <Scene/ParticleVolume.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/Sprite.hpp>
#include <Scene/TextSprite.hpp>
#include <Scene/Node.hpp>
#include <Scene/Prefab.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/FirstPersonCamera.hpp>

#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Core/Core.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Reflection/Enum.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <random>

#include <Asset/Assets.hpp>
#include <Asset/AssetBatch.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Framework/Gameplay/Weapon.hpp>

#include <Scene/Decal/Decal.hpp>

#include <System/OpenFileDialog.hpp>
#include <System/SaveFileDialog.hpp>
#include <System/SelectFolderDialog.hpp>
#include <System/MessageBox.hpp>
#include <System/AppContext.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/Overlays/Overlay.hpp>

#include <Baking/BakeLayer.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
ENGINE_API HYP_DECLARE_LOG_CHANNEL(Console);

extern Handle<EditorState> g_editorState;

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

Node* ResolveNodeUuidArgument(EditorSubsystem* subsystem, const String& nodeUuidArgument);

} // namespace Hyperion

#define DEFINE_EDITOR_COMMAND(name)                                        \
    const Class* g_clsEditorCommand##name = nullptr;                       \
                                                                           \
    const Class* EditorCommand##name ::StaticClass()                       \
    {                                                                      \
        return g_clsEditorCommand##name;                                   \
    }                                                                      \
                                                                           \
    HYP_BEGIN_CLASS(EditorCommand##name, -1, 0, NAME("EditorCommandBase")) \
    HYP_END_CLASS                                                          \
                                                                           \
    static TClassStaticInit<EditorCommand##name> g_classInit##EditorCommand##name {};
