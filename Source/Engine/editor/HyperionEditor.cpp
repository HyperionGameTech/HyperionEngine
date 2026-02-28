#include <EditorPch.hpp>

#include <editor/HyperionEditor.hpp>
#include <editor/EditorObjectProperties.hpp>
#include <editor/EditorDelegates.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorState.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/DebugDrawer.hpp>
#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <scene/World.hpp>
#include <scene/Light.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/FirstPersonCamera.hpp>

#include <scene/sky/DynamicSkySystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/AudioComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/RigidBodyComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/world_grid/terrain/TerrainWorldGridLayer.hpp>
#include <scene/world_grid/WorldGrid.hpp>

#include <script/HypScript.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>
#include <ui/UIListView.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIDataSource.hpp>
#include <ui/UIWindow.hpp>

#include <Core/config/Config.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/net/HTTPRequest.hpp>

#include <scripting/ScriptingService.hpp>

#include <Core/profiling/Profile.hpp>

#include <util/MeshBuilder.hpp>

#include <baking/BakerSubsystem.hpp>
#include <baking/BakeData.hpp>

#include <input/Event.hpp>

#include <system/AppContext.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <HyperionEngine.hpp>

#include <HyperionEditor.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static const Name s_nameEditorWorld = NAME("EditorWorld");

#pragma region HyperionEditor

HyperionEditor::HyperionEditor()
    : Game()
{
    m_world = MakeHandle<World>(s_nameEditorWorld, WorldFlags::EDITOR_WORLD);
}

HyperionEditor::~HyperionEditor()
{
}

void HyperionEditor::OnLaunch_Impl()
{
}

void HyperionEditor::OnUpdate_Impl(float delta)
{
}

void HyperionEditor::OnInputEvent(const Event& event)
{
    Game::OnInputEvent(event);
}

#pragma endregion HyperionEditor

} // namespace Hyperion
