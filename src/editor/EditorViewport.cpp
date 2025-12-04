#include <core/Defines.hpp>

#include <editor/EditorViewport.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <system/AppContext.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <EditorViewport.generated.inl>

namespace hyperion {

EditorViewport::EditorViewport()
{
}

EditorViewport::EditorViewport(const Handle<View>& view, const Handle<ApplicationWindow>& window)
    : m_view(view),
      m_window(window)
{
}

EditorViewport::~EditorViewport()
{
    SafeDelete(std::move(m_view));
    SafeDelete(std::move(m_window));
}

void EditorViewport::Init()
{
    ObjectBase::Init();

    if (!m_view)
    {
        m_view = CreateObject<View>();
    }

    InitObject(m_view);

    SetReady(true);
}

void EditorViewport::OnAdded(EditorSubsystem* editorSubsystem)
{
    AssertReady();

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();

    if (!currentProject)
    {
        return;
    }

    for (const Handle<Scene>& scene : currentProject->GetWorld()->GetScenes())
    {
        Assert(scene != nullptr);

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        m_view->AddScene(scene);
    }

    currentProject->GetWorld()->AddView(m_view);
}

void EditorViewport::OnRemoved(EditorSubsystem* editorSubsystem)
{
    AssertReady();

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();

    if (!currentProject)
    {
        return;
    }

    for (const Handle<Scene>& scene : currentProject->GetWorld()->GetScenes())
    {
        Assert(scene != nullptr);

        m_view->RemoveScene(scene);
    }

    currentProject->GetWorld()->RemoveView(m_view);
}

void EditorViewport::OnSceneAdded(Scene* scene)
{
    AssertReady();
    Assert(scene != nullptr);

    if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
    {
        return;
    }

    m_view->AddScene(MakeStrongRef(scene));
}

void EditorViewport::OnSceneRemoved(Scene* scene)
{
    AssertReady();
    Assert(scene != nullptr);

    m_view->RemoveScene(scene);
}

} // namespace hyperion
