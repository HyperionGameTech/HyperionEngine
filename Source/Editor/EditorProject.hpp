/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectBase.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Time.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Core/Name/Name.hpp>

#include <Asset/AssetPath.hpp>

#include <Rendering/RenderableAttributes.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Baking/BakeLayer.hpp>

#include <Editor/EditorMemory.hpp>
#include <Editor/EditorPackagingSettings.hpp>
#include <Editor/EditorProjectSettings.hpp>

namespace Hyperion {

namespace dotnet {
class Assembly;
} // namespace dotnet

class Scene;
class World;
class Game;
class EditorActionStack;
class EditorSubsystem;
class EditorAssetFolders;

using Baking::BakeLayer;

HYP_CLASS()
class EDITOR_API EditorProject final : public ObjectBase
{
    HYP_OBJECT_BODY(EditorProject);

public:
    friend class EditorSubsystem;

    static Pool* GetAllocator() { return g_editorPool; }

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

    /// If simulation is active, will return Game::GetWorld() for the game instance
    /// Otherwise, returns the transient edit-mode world
    HYP_METHOD()
    const Handle<World>& GetWorld() const;

    /// Set the transient edit-mode world
    void SetEditWorld(const Handle<World>& world)
    {
        m_editWorld = world;
    }

    /// Name of the World asset opened for editing
    HYP_METHOD()
    Name GetEditWorldName() const;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<EditorProjectSettings>& GetProjectSettings() const
    {
        return m_projectSettings;
    }

    HYP_FORCE_INLINE const EditorPlatformSettings& GetPlatformSettings() const
    {
        return m_projectSettings->platformSettings;
    }

    HYP_METHOD()
    Array<Name> GetEnabledPlugins() const;

    /*! \brief The runtime modules of the enabled plugins and their dependencies, which a packaged or web build of the game links */
    HYP_METHOD()
    Array<Name> GetEnabledPluginRuntimeModules() const;

    HYP_METHOD()
    String GetGameTitle() const;

    HYP_METHOD()
    String GetModuleName() const;

    HYP_METHOD()
    String GetModuleNameFromTitle() const;

    HYP_METHOD()
    bool HasGameCode() const;

    HYP_METHOD()
    Result WriteGameArgs() const;

    HYP_METHOD()
    Result RenameGameCode(const String& newModuleName);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<PackagingSettings>& GetPackagingSettings() const
    {
        return m_packagingSettings;
    }

    /// For edits outside the asset registry, which IsDirty() can't see
    HYP_METHOD()
    void MarkSettingsDirty()
    {
        m_hasUnsavedSettings = true;
    }

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

    /// While the project has not yet been saved, its content is kept in a temp directory,
    /// which is removed once the project is saved or discarded
    HYP_METHOD()
    HYP_FORCE_INLINE const FilePath& GetTempDirectory() const
    {
        return m_tempDirectory;
    }

    HYP_METHOD()
    FilePath GetProjectDirectory() const;

    HYP_METHOD()
    FilePath GetContentDirectory() const;

    HYP_METHOD()
    HYP_FORCE_INLINE const String& GetContentSubfolder() const
    {
        return m_contentSubfolder;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Vec3f& GetEditorCameraPosition() const
    {
        return m_editorCameraPosition;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Vec3f& GetEditorCameraDirection() const
    {
        return m_editorCameraDirection;
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
    bool IsDirty() const;

    HYP_METHOD()
    const Handle<EditorActionStack>& GetActionStack() const
    {
        return m_actionStack;
    }

    HYP_METHOD()
    const Handle<EditorAssetFolders>& GetAssetFolders() const
    {
        return m_assetFolders;
    }

    BakeLayer& GetActiveBakeLayer();

    /// For editor interop
    HYP_METHOD()
    Array<Name> GetSwatchNames() const;

    HYP_METHOD()
    Name GetActiveSwatchName() const;

    HYP_METHOD()
    void SetActiveSwatch(Name swatchName);

    static TResult<Handle<EditorProject>> Load(const FilePath& filepath);
    static Handle<EditorProject> CreateNew();

    HYP_METHOD()
    void Close(bool shutdownWorld = true);

    HYP_FIELD()
    ScriptableDelegate<void, const Handle<EditorProject>&> OnProjectSaved;

    HYP_FIELD()
    ScriptableDelegate<void, Name> OnActiveSwatchChanged;

private:
    HYP_FORCE_INLINE void SetEditorSubsystem(const WeakHandle<EditorSubsystem>& editorSubsystem)
    {
        m_editorSubsystem = editorSubsystem;
    }

    HYP_FIELD(Property = "Name", Serialize)
    Name m_name;

    HYP_FIELD(Property = "LastSavedTime", Transient)
    Time m_lastSavedTime;

    HYP_FIELD(Property = "FilePath", Transient)
    FilePath m_filepath;

    HYP_FIELD(Property = "TempDirectory", Transient)
    FilePath m_tempDirectory;

    HYP_FIELD(Property = "ContentSubfolder", Serialize)
    String m_contentSubfolder;

    HYP_FIELD(Property = "GameInstance", Serialize)
    Handle<Game> m_gameInstance;

    HYP_FIELD(Property = "EditorCameraPosition", Serialize)
    Vec3f m_editorCameraPosition;

    HYP_FIELD(Property = "EditorCameraDirection", Serialize)
    Vec3f m_editorCameraDirection;

    HYP_FIELD(Property = "EditWorldName", Serialize)
    Name m_editWorldName;

    HYP_FIELD(Property = "ProjectSettings", Serialize)
    Handle<EditorProjectSettings> m_projectSettings;

    // read from files saved before ProjectSettings held it, never written
    HYP_FIELD(Property = "PlatformSettings", Transient)
    EditorPlatformSettings m_legacyPlatformSettings;

    HYP_FIELD(Property = "PackagingSettings", Serialize)
    Handle<PackagingSettings> m_packagingSettings;

    bool m_hasUnsavedSettings = false;

    HYP_FIELD(Transient)
    Handle<EditorActionStack> m_actionStack;

    HYP_FIELD(Transient)
    Handle<EditorAssetFolders> m_assetFolders;

    // Transient world set while in editor mode
    Handle<World> m_editWorld;

    // The project's C# scripts, loaded with the project so their classes resolve while its scenes are read
    SharedPtr<dotnet::Assembly> m_scriptAssembly;

    WeakHandle<EditorSubsystem> m_editorSubsystem;
};

} // namespace Hyperion
