/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/reflection/ObjectBase.hpp>

#include <Core/functional/Delegate.hpp>
#include <scripting/ScriptableDelegate.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/utilities/Time.hpp>

#include <Core/name/Name.hpp>

namespace Hyperion {

class Scene;
class World;
class Game;
class EditorActionStack;
class EditorSubsystem;

HYP_CLASS()
class EditorProject final : public ObjectBase
{
    HYP_OBJECT_BODY(EditorProject);

public:
    friend class EditorSubsystem;

    EditorProject();

    explicit EditorProject(const Handle<Game>& gameInstance);
    EditorProject(Name name, const Handle<Game>& gameInstance);

    EditorProject(const EditorProject& other) = delete;
    EditorProject& operator=(const EditorProject& other) = delete;

    virtual ~EditorProject() override;

    HYP_FORCE_INLINE const WeakHandle<EditorSubsystem>& GetEditorSubsystem() const
    {
        return m_editorSubsystem;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    void SetName(Name name);

    HYP_METHOD()
    const Handle<World>& GetWorld() const;

    HYP_METHOD(Property = "GameInstance")
    HYP_FORCE_INLINE const Handle<Game>& GetGame() const
    {
        return m_gameInstance;
    }

    HYP_METHOD(Property = "GameInstance")
    void SetGame(const Handle<Game>& gameInstance);

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
    void AddScene(const Handle<Scene>& scene);

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
    static Handle<EditorProject> CreateNew();

    HYP_METHOD()
    void Close(bool shutdownWorld = true);

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<EditorProject>&> OnProjectSaved;

private:
    HYP_FORCE_INLINE void SetEditorSubsystem(const WeakHandle<EditorSubsystem>& editorSubsystem)
    {
        m_editorSubsystem = editorSubsystem;
    }

    Name GetNextDefaultProjectName_Impl(const String& defaultProjectName) const;

    HYP_FIELD(Property = "Name")
    Name m_name;

    HYP_FIELD(Property = "LastSavedTime", Transient)
    Time m_lastSavedTime;

    HYP_FIELD(Property = "FilePath", Transient)
    FilePath m_filepath;

    HYP_FIELD(Property = "GameInstance")
    Handle<Game> m_gameInstance;

    HYP_FIELD(Transient)
    Handle<EditorActionStack> m_actionStack;

    WeakHandle<EditorSubsystem> m_editorSubsystem;
};

} // namespace Hyperion
