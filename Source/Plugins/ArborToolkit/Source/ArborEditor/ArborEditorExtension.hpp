/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorExtension.hpp>

namespace Hyperion {

class EditorArborState;

HYP_CLASS(Serialize = false)
class ARBOREDITOR_API ArborEditorExtension final : public EditorExtension
{
    HYP_OBJECT_BODY(ArborEditorExtension);

public:
    ArborEditorExtension();
    ~ArborEditorExtension() override;

    HYP_METHOD()
    const Handle<EditorArborState>& GetArborState() const;

    void OnAttached() override;
    void OnDetached() override;

    void Update(float delta) override;

private:
    Handle<EditorArborState> m_arborState;
};

} // namespace Hyperion
