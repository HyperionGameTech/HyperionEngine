/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <ArborEditor/ArborEditorExtension.hpp>
#include <ArborEditor/EditorArborState.hpp>
#include <Editor/EditorSubsystem.hpp>

#include <ArborEditorExtension.generated.inl>

namespace Hyperion {

ArborEditorExtension::ArborEditorExtension() = default;

ArborEditorExtension::~ArborEditorExtension() = default;

const Handle<EditorArborState>& ArborEditorExtension::GetArborState() const
{
    return m_arborState;
}

void ArborEditorExtension::OnAttached()
{
    m_arborState = MakeHandle<EditorArborState>();
    InitObject(m_arborState);

    m_arborState->Initialize(GetEditorSubsystem());
}

void ArborEditorExtension::OnDetached()
{
    if (m_arborState.IsValid())
    {
        m_arborState->Shutdown();
    }

    m_arborState.Reset();
}

void ArborEditorExtension::Update(float delta)
{
    if (m_arborState.IsValid())
    {
        m_arborState->Update();
    }
}

} // namespace Hyperion
