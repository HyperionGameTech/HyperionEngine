/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <core/threading/Mutex.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <Types.hpp>

namespace hyperion {

class EditorProject;

HYP_CLASS()
class HYP_API EditorState : public HypObjectBase
{
    HYP_OBJECT_BODY(EditorState);

public:
    ~EditorState() override = default;

    HYP_METHOD()
    Handle<EditorProject> GetCurrentProject() const;

    HYP_METHOD()
    void SetCurrentProject(const Handle<EditorProject>& project);

    HYP_FIELD()
    ScriptableDelegate<void, Handle<EditorProject>> OnCurrentProjectChanged;

private:
    void Init() override;

    void ImportAssetsOrSetCallback(const Handle<EditorProject>& project);

    Handle<EditorProject> m_currentProject;
    mutable Mutex m_mutex;
};

} // namespace hyperion

