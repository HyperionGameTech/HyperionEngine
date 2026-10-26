/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorExtension.hpp>

namespace Hyperion {

HYP_CLASS(Serialize = false)
class PluginTestEditorExtension final : public EditorExtension
{
    HYP_OBJECT_BODY(PluginTestEditorExtension);

public:
    HYP_FORCE_INLINE uint32 GetNumUpdates() const
    {
        return m_numUpdates;
    }

    void OnAttached() override;
    void OnDetached() override;
    void Update(float delta) override;

private:
    uint32 m_numUpdates = 0;
};

} // namespace Hyperion
