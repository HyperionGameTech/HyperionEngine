/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/System.hpp>
#include <Scene/Components/ScriptComponent.hpp>

#include <Framework/GameState.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Functional/Delegate.hpp>
#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

namespace Hyperion {

namespace dotnet {
class Assembly;
} // namespace dotnet

class ScriptingService;
class ScriptTracker;
class AssetRegistry;

HYP_CLASS(NoScriptBindings, Serialize=false)
class ScriptSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ScriptSystem);

public:
    ScriptSystem();
    ~ScriptSystem() override;

    bool AllowParallelExecution() const override;
    bool RequiresSimThread() const override;
    bool AllowUpdate() const override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void OnEntityAdded(Entity* entity) override;
    void OnEntityRemoved(Entity* entity) override;

    void Process(float delta, Span<Handle<Scene>> scenes) override;

    void RefreshScriptSourceDirectories();

    /*! \brief Recreates the script object of an entity whose ScriptComponent changed script after it was added */
    void RecreateEntityScript(Entity* entity);

    /*! \brief Builds (if needed) and loads the C# module of a project's Scripts folder, so the classes it declares
     *  (e.g C# subclasses of Entity) are registered before the project's scenes are read. Editor only; null otherwise. */
    ENGINE_API static SharedPtr<dotnet::Assembly> LoadProjectScriptAssembly(
        const FilePath& contentDirectory,
        Span<const Name> pluginModules);

    ENGINE_API static void RetargetReloadedObjects(World& world);

    ENGINE_API static void SetGameAssembly(const FilePath& projectDirectory, const FilePath& assemblyPath, bool isNewBuild);

    static Delegate<void> OnScriptReloaded;

private:
    SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<ScriptComponent, ComponentAccess::READ_WRITE> {}
        };
    }

    Array<FilePath> CollectScriptSourceDirectories() const;

    void HandleGameStateChanged(GameStateMode gameStateMode, GameStateMode previousGameStateMode);

    // Builds the C# module for a script if needed and points its asset at the assembly
    void ResolveScriptAssembly(ScriptComponent& scriptComponent);

    void ApplyGameAssembly(bool rebuildOnReload);

    UniquePtr<ScriptingService> m_scriptingService;
    UniquePtr<ScriptTracker> m_scriptTracker;

    uint32 m_gameAssemblyVersion = 0;
    FilePath m_gameProjectDirectory;
};

} // namespace Hyperion
