/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorIsolatedEditState.hpp>

namespace Hyperion {

class Prefab;

HYP_CLASS(Serialize = false)
class EDITOR_API EditorPrefabEditState final : public EditorIsolatedEditState
{
    HYP_OBJECT_BODY(EditorPrefabEditState);

public:
    EditorPrefabEditState();
    ~EditorPrefabEditState() override;

    bool IsActive() const override;

    HYP_METHOD()
    bool Enter(const Handle<Prefab>& prefab);

    bool Apply() override;
    void Revert() override;

    String GetDisplayName() const override;
    String GetKindName() const override;

    HYP_METHOD()
    const Handle<Prefab>& GetPrefab() const;

    HYP_METHOD()
    String GetPrefabName() const;

    HYP_METHOD()
    uint32 GetNumLiveInstances() const;

    HYP_METHOD()
    bool IsUpdateInstancesOnApply() const;

    HYP_METHOD()
    void SetUpdateInstancesOnApply(bool updateInstancesOnApply);

private:
    void ClearTarget() override;

    Handle<Node> CloneWorkingRoot() const;
    void SetWorkingRoot(const Handle<Node>& workingRoot);

    Handle<Prefab> m_prefab;

    bool m_updateInstancesOnApply = true;
};

} // namespace Hyperion
