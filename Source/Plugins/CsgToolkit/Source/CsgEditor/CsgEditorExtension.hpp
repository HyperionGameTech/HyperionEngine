/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorExtension.hpp>

namespace Hyperion {

class EditorCsgState;

HYP_CLASS(Serialize = false)
class CSGEDITOR_API CsgEditorExtension final : public EditorExtension
{
    HYP_OBJECT_BODY(CsgEditorExtension);

public:
    CsgEditorExtension();
    ~CsgEditorExtension() override;

    HYP_METHOD()
    const Handle<EditorCsgState>& GetCsgState() const;

    void OnAttached() override;
    void OnDetached() override;

    void Update(float delta) override;

    void DebugDraw(DebugDrawCommandList& debugDrawCommandList) override;

    bool OnViewportClick(const MouseEvent& event, const Ray& ray) override;
    bool OnViewportMouseDown(const MouseEvent& event) override;
    bool OnViewportMouseDrag(const MouseEvent& event) override;
    void OnViewportMouseMove(const MouseEvent& event) override;
    bool OnViewportKeyDown(const KeyboardEvent& event) override;
    void EndViewportDrag() override;

    bool IsToolActive() const override;
    void ExitTool() override;

    EditorManipulationMode ResolveManipulationMode(EditorManipulationMode requestedMode) override;

    bool GetGizmoTargetNodes(Array<Handle<Node>>& outTargetNodes) const override;

    void OnFocusedNodeChanged(const Handle<Node>& focusedNode) override;

private:
    Handle<EditorCsgState> m_csgState;
};

} // namespace Hyperion
