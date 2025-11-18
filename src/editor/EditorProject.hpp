/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/reflection/ObjectBase.hpp>

#include <core/functional/Delegate.hpp>
#include <core/functional/ScriptableDelegate.hpp>

#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <core/utilities/Time.hpp>

#include <core/Name.hpp>

namespace hyperion {

class Scene;
class World;
class AssetCollector;
class AssetPackage;
class EditorActionStack;
class EditorSubsystem;

HYP_CLASS()
class HYP_API EditorProject final : public ObjectBase
{
    HYP_OBJECT_BODY(EditorProject);

public:
    friend class EditorSubsystem;

    EditorProject();

    explicit EditorProject(Name name);

    EditorProject(const EditorProject& other) = delete;
    EditorProject& operator=(const EditorProject& other) = delete;

    virtual ~EditorProject() override;

    HYP_FORCE_INLINE const WeakHandle<EditorSubsystem>& GetEditorSubsystem() const
    {
        return m_editorSubsystem;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Uuid& GetUUID() const
    {
        return m_uuid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    void SetName(Name name);

    HYP_METHOD(Property = "World")
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Time GetLastSavedTime() const
    {
        return m_lastSavedTime;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const FilePath& GetFilePath() const
    {
        return m_filepath;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<AssetPackage>& GetPackage() const
    {
        return m_package;
    }

    HYP_METHOD()
    void AddScene(const Handle<Scene>& scene, Vec2i streamingCoord);

    HYP_METHOD()
    void RemoveScene(Scene* scene);

    HYP_METHOD()
    FilePath GetProjectsDirectory() const;

    HYP_METHOD()
    bool IsSaved() const;

    HYP_METHOD()
    Result Save();

    HYP_METHOD()
    Result SaveAs(FilePath filepath);

    HYP_METHOD()
    const Handle<EditorActionStack>& GetActionStack() const
    {
        return m_actionStack;
    }

    static TResult<Handle<EditorProject>> Load(const FilePath& filepath);

    HYP_METHOD()
    void Close();

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

    HYP_FIELD(Property = "UUID")
    Uuid m_uuid;

    HYP_FIELD(Property = "Name")
    Name m_name;

    HYP_FIELD(Property = "LastSavedTime")
    Time m_lastSavedTime;

    HYP_FIELD(Property = "FilePath")
    FilePath m_filepath;

    HYP_FIELD(Property = "World")
    Handle<World> m_world;

    HYP_FIELD(Transient)
    Handle<AssetPackage> m_package;

    HYP_FIELD(Transient)
    Handle<EditorActionStack> m_actionStack;

    WeakHandle<EditorSubsystem> m_editorSubsystem;
};

} // namespace hyperion
