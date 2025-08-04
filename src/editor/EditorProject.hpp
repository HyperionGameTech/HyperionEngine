/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>
#include <core/object/HypClassUtils.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/ScriptableDelegate.hpp>

#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <core/utilities/Time.hpp>

#include <core/Name.hpp>

namespace hyperion {

class Scene;
class AssetCollector;
class AssetPackage;
class EditorActionStack;
class EditorSubsystem;

HYP_CLASS()
class HYP_API EditorProject final : public HypObjectBase
{
    HYP_OBJECT_BODY(EditorProject);

public:
    friend class EditorSubsystem;

    EditorProject();
    EditorProject(Name name);
    EditorProject(const EditorProject& other) = delete;
    EditorProject& operator=(const EditorProject& other) = delete;
    virtual ~EditorProject() override;

    HYP_FORCE_INLINE const WeakHandle<EditorSubsystem>& GetEditorSubsystem() const
    {
        return m_editorSubsystem;
    }

    HYP_METHOD(Property = "UUID", Serialize = true)
    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    /*! \internal For serialization only. */
    HYP_METHOD(Property = "UUID", Serialize = true)
    HYP_FORCE_INLINE void SetUUID(const UUID& uuid)
    {
        m_uuid = uuid;
    }

    HYP_METHOD(Property = "Name", Serialize = true)
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name", Serialize = true)
    void SetName(Name name);

    HYP_METHOD(Property = "LastSavedTime", Serialize = true)
    HYP_FORCE_INLINE Time GetLastSavedTime() const
    {
        return m_lastSavedTime;
    }

    /*! \internal For serialization only. */
    HYP_METHOD(Property = "LastSavedTime", Serialize = true)
    HYP_FORCE_INLINE void SetLastSavedTime(Time lastSavedTime)
    {
        m_lastSavedTime = lastSavedTime;
    }

    HYP_METHOD(Property = "FilePath", Serialize = true)
    HYP_FORCE_INLINE const FilePath& GetFilePath() const
    {
        return m_filepath;
    }

    HYP_METHOD(Property = "FilePath", Serialize = true)
    HYP_FORCE_INLINE void SetFilePath(const FilePath& filepath)
    {
        m_filepath = filepath;
    }

    HYP_METHOD(Property = "Scenes")
    HYP_FORCE_INLINE const Array<Handle<Scene>>& GetScenes() const
    {
        return m_scenes;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<AssetPackage>& GetPackage() const
    {
        return m_package;
    }

    HYP_METHOD()
    void AddScene(const Handle<Scene>& scene);

    HYP_METHOD()
    void RemoveScene(const Handle<Scene>& scene);

    HYP_METHOD()
    FilePath GetProjectsDirectory() const;

    HYP_METHOD()
    bool IsSaved() const;

    HYP_METHOD()
    Result Save();

    HYP_METHOD()
    Result SaveAs(FilePath filepath);

    HYP_METHOD(Scriptable)
    Name GetNextDefaultProjectName(const String& defaultProjectName) const;

    HYP_METHOD()
    const Handle<EditorActionStack>& GetActionStack() const
    {
        return m_actionStack;
    }

    static TResult<Handle<EditorProject>> Load(const FilePath& filepath);

    HYP_METHOD()
    void Close();

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<Scene>&> OnSceneAdded;

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<Scene>&> OnSceneRemoved;

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<EditorProject>&> OnProjectSaved;
    
    Delegate<void, Handle<AssetPackage>> OnPackageCreated;

private:
    void Init() override;

    Result CreatePackage();

    HYP_FORCE_INLINE void SetEditorSubsystem(const WeakHandle<EditorSubsystem>& editorSubsystem)
    {
        m_editorSubsystem = editorSubsystem;
    }

    Name GetNextDefaultProjectName_Impl(const String& defaultProjectName) const;

    UUID m_uuid;

    Name m_name;

    Time m_lastSavedTime;

    FilePath m_filepath;

    HYP_FIELD(Property = "Scenes", Serialize = true)
    Array<Handle<Scene>> m_scenes;

    HYP_FIELD(Property = "Package", Serialize = true)
    Handle<AssetPackage> m_package;

    Handle<EditorActionStack> m_actionStack;

    WeakHandle<EditorSubsystem> m_editorSubsystem;
};

} // namespace hyperion

