/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/EditorPickCache.hpp>

#include <core/reflection/Handle.hpp>
#include <core/reflection/HypObject.hpp>

#include <core/threading/Mutex.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/Types.hpp>

namespace hyperion {

class EditorProject;

HYP_CLASS()
class HYP_API EditorState : public HypObjectBase
{
    HYP_OBJECT_BODY(EditorState);

public:
    EditorState();
    ~EditorState() override;

    HYP_FORCE_INLINE EditorPickCache& GetPickCache()
    {
        return m_pickCache;
    }

    HYP_FORCE_INLINE const EditorPickCache& GetPickCache() const
    {
        return m_pickCache;
    }

    HYP_METHOD()
    Handle<EditorProject> GetCurrentProject() const;

    HYP_METHOD()
    void SetCurrentProject(const Handle<EditorProject>& project);

    void Update(float delta);

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorProject>> OnCurrentProjectChanged;

private:
    void Init() override;

    void ImportAssetsOrSetCallback(const Handle<EditorProject>& project);

    Handle<EditorProject> m_currentProject;
    EditorPickCache m_pickCache;
    mutable Mutex m_mutex;

    DelegateHandler m_onAssetObjectAddedHandle;
    DelegateHandler m_onProjectPackageChangedHandle;
};

} // namespace hyperion
