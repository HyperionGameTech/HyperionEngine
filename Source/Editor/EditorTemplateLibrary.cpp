/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorTemplateLibrary.hpp>

#include <Scene/Node.hpp>
#include <Scene/Prefab.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetBucket.hpp>

#include <System/DirectoryInitializer.hpp>

#include <Core/Containers/Set.hpp>
#include <Core/Containers/StaticString.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

static constexpr const char* TemplateIndexFilename = "Template.json";

static FilePath GetTemplatePrefabPath(const FilePath& templateDirectory, const String& templateName)
{
    return templateDirectory / AssetBuckets::Prefabs.GetName() / (templateName + ".hmf");
}

static void UntagPrefabInstances(Node* root)
{
    Prefab::UntagAsPrefabInstance(root);

    for (Node* descendant : root->GetDescendants())
    {
        Prefab::UntagAsPrefabInstance(descendant);
    }
}

static Array<Handle<AssetObject>> CollectDependencies(const Handle<Prefab>& prefab)
{
    Array<Handle<AssetObject>> dependencies;

    const Handle<AssetObject> prefabAsset = prefab;

    auto onAssetFound = [&](const Handle<AssetObject>& asset)
    {
        if (asset.Get() != prefab.Get())
        {
            dependencies.PushBack(asset);
        }
    };

    AssetRegistry::WalkAssetDeep(BoxedValue(prefabAsset), onAssetFound);

    return dependencies;
}

const FilePath& EditorTemplateLibrary::GetDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Templates")> s_templatesDirectory;
    return s_templatesDirectory.path;
}

Array<Name> EditorTemplateLibrary::GetTemplateNames()
{
    Array<Name> templateNames;

    for (const FilePath& templateDirectory : GetDirectory().GetSubdirectories())
    {
        const String templateName = templateDirectory.Basename();

        if (!IsValidTemplateName(templateName) || !GetTemplatePrefabPath(templateDirectory, templateName).Exists())
        {
            continue;
        }

        templateNames.PushBack(CreateNameFromDynamicString(ANSIString(templateName)));
    }

    return templateNames;
}

bool EditorTemplateLibrary::HasTemplate(Name templateName)
{
    if (!templateName.IsValid())
    {
        return false;
    }

    const String templateNameString = *templateName;

    return GetTemplatePrefabPath(GetDirectory() / templateNameString, templateNameString).Exists();
}

bool EditorTemplateLibrary::IsValidTemplateName(const String& templateName)
{
    if (templateName.Empty())
    {
        return false;
    }

    for (auto character : templateName)
    {
        const bool isAllowed = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '_'
            || character == '-';

        if (!isAllowed)
        {
            return false;
        }
    }

    return true;
}

Result EditorTemplateLibrary::SaveTemplate(Name templateName, const Handle<Node>& root)
{
    AssertOnThread(g_simThread);

    if (!templateName.IsValid() || !IsValidTemplateName(*templateName))
    {
        return HYP_MAKE_ERROR(Error, "Invalid template name '{}'", templateName);
    }

    if (!root.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "No nodes to save as template '{}'", templateName);
    }

    Handle<AssetRegistry> projectRegistry = GetCurrentAssetRegistry();

    if (!projectRegistry.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "No asset registry to save template '{}' from", templateName);
    }

    const String templateNameString = *templateName;

    UntagPrefabInstances(root.Get());

    Handle<Prefab> templatePrefab = MakeHandle<Prefab>(templateName, root);
    InitObject(templatePrefab);

    Array<Handle<AssetObject>> dependencies = CollectDependencies(templatePrefab);

    for (const Handle<AssetObject>& dependency : dependencies)
    {
        if (!dependency->IsRegistered() && !dependency->IsTransient())
        {
            projectRegistry->PutAssetUnique(dependency);
        }
    }

    const FilePath stagingDirectory = GetDirectory() / (templateNameString + ".saving");

    if (stagingDirectory.Exists() && !stagingDirectory.RemoveRecursively())
    {
        return HYP_MAKE_ERROR(Error, "Failed to clear leftover template directory '{}'", stagingDirectory);
    }

    JSON::JArray assetEntries;

    for (const Handle<AssetObject>& dependency : dependencies)
    {
        const AssetPath& assetPath = dependency->GetPath();

        if (!dependency->IsRegistered() || assetPath.registryId != AssetRegistryId::Game)
        {
            continue;
        }

        const AssetBucket& bucket = assetPath.GetBucket();

        if (bucket == AssetBuckets::Prefabs && dependency->GetName() == templateName)
        {
            (void)stagingDirectory.RemoveRecursively();

            return HYP_MAKE_ERROR(Error, "Template '{}' uses a Prefab of the same name; pick a different template name", templateName);
        }

        if (Result exportResult = dependency->ExportFiles(stagingDirectory / bucket.GetName()); exportResult.HasError())
        {
            (void)stagingDirectory.RemoveRecursively();

            return exportResult;
        }

        JSON::Object assetEntry;
        assetEntry.Set("bucket", JSON::JString(bucket.GetName()));
        assetEntry.Set("name", JSON::JString(*dependency->GetName()));
        assetEntry.Set("uuid", JSON::JString(dependency->GetUUID().ToString()));

        assetEntries.PushBack(JSON::Value(std::move(assetEntry)));
    }

    if (Result exportResult = templatePrefab->ExportFiles(stagingDirectory / AssetBuckets::Prefabs.GetName()); exportResult.HasError())
    {
        (void)stagingDirectory.RemoveRecursively();

        return exportResult;
    }

    JSON::Object index;
    index.Set("name", JSON::JString(templateNameString));
    index.Set("assets", JSON::Value(std::move(assetEntries)));

    {
        FileByteWriter indexWriter { stagingDirectory / TemplateIndexFilename };

        if (!indexWriter.IsOpen())
        {
            (void)stagingDirectory.RemoveRecursively();

            return HYP_MAKE_ERROR(Error, "Failed to write index for template '{}'", templateName);
        }

        indexWriter.WriteString(JSON::Value(std::move(index)).ToString(true).ToUtf8());
        indexWriter.Close();
    }

    const FilePath templateDirectory = GetDirectory() / templateNameString;

    if (templateDirectory.Exists() && !templateDirectory.RemoveRecursively())
    {
        (void)stagingDirectory.RemoveRecursively();

        return HYP_MAKE_ERROR(Error, "Failed to replace existing template '{}'", templateName);
    }

    if (!stagingDirectory.Rename(templateDirectory))
    {
        return HYP_MAKE_ERROR(Error, "Failed to move template '{}' into place from '{}'", templateName, stagingDirectory);
    }

    HYP_LOG(Editor, Info, "Saved template '{}' with {} project asset(s) to '{}'", templateName, dependencies.Size(), templateDirectory);

    return {};
}

TResult<Handle<Node>> EditorTemplateLibrary::InstantiateTemplate(Name templateName, Array<Handle<AssetObject>>* outImportedAssets)
{
    AssertOnThread(g_simThread);

    if (!HasTemplate(templateName))
    {
        return HYP_MAKE_ERROR(Error, "No template named '{}' in '{}'", templateName, GetDirectory());
    }

    Handle<AssetRegistry> projectRegistry = GetCurrentAssetRegistry();

    if (!projectRegistry.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "No asset registry to instantiate template '{}' into", templateName);
    }

    const FilePath templateDirectory = GetDirectory() / *templateName;

    Handle<AssetRegistry> templateRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, templateDirectory);
    templateRegistry->Initialize();

    Array<Handle<AssetObject>> reusedAssets;

    {
        FileByteReader indexReader { templateDirectory / TemplateIndexFilename };

        JSON::ParseResult indexParseResult;
        indexParseResult.ok = false;

        if (!indexReader.Eof())
        {
            indexParseResult = JSON::Parse(indexReader);
        }

        if (indexParseResult.ok && indexParseResult.value["assets"].IsArray())
        {
            for (const JSON::Value& assetEntry : indexParseResult.value["assets"].AsArray())
            {
                const AssetBucket& bucket = GetAssetBucketByName(StringHash(assetEntry["bucket"].ToString()));

                if (bucket == AssetBuckets::None)
                {
                    continue;
                }

                const Name assetName = CreateNameFromDynamicString(ANSIString(assetEntry["name"].ToString()));
                const UUID assetUuid = UUID(ANSIString(assetEntry["uuid"].ToString()).Data());

                Handle<AssetObject> existingAsset = projectRegistry->GetAsset(bucket, assetName);

                if (existingAsset.IsValid() && existingAsset->GetUUID() == assetUuid)
                {
                    templateRegistry->SetRedirect(bucket, assetName, existingAsset);
                    reusedAssets.PushBack(existingAsset);
                }
            }
        }
        else
        {
            HYP_LOG(Editor, Warning, "Template '{}' has no readable {}; all of its assets will be imported as new copies", templateName, TemplateIndexFilename);
        }
    }

    Set<const AssetObject*> projectOwnedAssets;

    auto onProjectOwnedAssetFound = [&](const Handle<AssetObject>& asset)
    {
        projectOwnedAssets.Insert(asset.Get());
    };

    for (const Handle<AssetObject>& reusedAsset : reusedAssets)
    {
        AssetRegistry::WalkAssetDeep(BoxedValue(reusedAsset), onProjectOwnedAssetFound);
    }

    Handle<Node> root;
    Array<Handle<AssetObject>> importedAssets;

    {
        GlobalContextScope assetRegistryScope { AssetRegistryContext { templateRegistry } };

        Handle<Prefab> templatePrefab = templateRegistry->GetAsset<Prefab>(AssetBuckets::Prefabs, templateName);

        if (!templatePrefab.IsValid() || !templatePrefab->GetRoot().IsValid())
        {
            templateRegistry->Shutdown();

            return HYP_MAKE_ERROR(Error, "Failed to load template '{}'", templateName);
        }

        for (const Handle<AssetObject>& dependency : CollectDependencies(templatePrefab))
        {
            if (dependency->IsRegistered()
                && dependency->GetPath().registryId == AssetRegistryId::Game
                && !projectOwnedAssets.Contains(dependency.Get()))
            {
                importedAssets.PushBack(dependency);
            }
        }

        for (const Handle<AssetObject>& importedAsset : importedAssets)
        {
            importedAsset->LockReader();
        }

        for (const Handle<AssetObject>& importedAsset : importedAssets)
        {
            templateRegistry->ReleaseAsset(importedAsset);
        }

        root = templatePrefab->GetRoot()->Clone();
    }

    // Name clashes with unrelated project assets get a numeric suffix here
    for (const Handle<AssetObject>& importedAsset : importedAssets)
    {
        projectRegistry->PutAssetUnique(importedAsset);
    }

    for (const Handle<AssetObject>& importedAsset : importedAssets)
    {
        importedAsset->UnlockReader();
    }

    templateRegistry->Shutdown();

    if (!root.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Failed to clone the nodes of template '{}'", templateName);
    }

    HYP_LOG(Editor, Info, "Instantiated template '{}': imported {} asset(s), reused {}", templateName, importedAssets.Size(), reusedAssets.Size());

    if (outImportedAssets != nullptr)
    {
        *outImportedAssets = std::move(importedAssets);
    }

    return root;
}

} // namespace Hyperion
