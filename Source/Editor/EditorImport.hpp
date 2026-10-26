/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/Loader.hpp>
#include <Asset/ModelImport.hpp>

#include <Core/FileSystem/FilePath.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Utilities/StringUtil.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Functional/Proc.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class EditorSubsystem;
class AssetObject;

using EditorImportCallback = Proc<void(EditorSubsystem&, const Array<Handle<AssetObject>>&)>;

/// Imported assets are filed under targetFolder.
/// a model gets its own <targetFolder>/<ModelName>/ with a subfolder per asset type
/// with reviewModels, models with a skeleton or animations wait in a EditorModelImportReview for their settings
void ImportEditorContent(
    EditorSubsystem* subsystem,
    const Array<FilePath>& files,
    const String& targetFolder,
    EditorImportCallback&& onImported = {},
    bool reviewModels = false);

/// Registers a model whose settings are already applied and files its assets under targetFolder
Array<Handle<AssetObject>> CommitEditorModelImport(EditorSubsystem& subsystem, const ModelImportContents& contents, const String& targetFolder);

const AssetImportInfo* GetEditorAssetImportInfo(const AssetObject* asset);

/// Reruns the import that made asset with its stored settings, keeping the identities of the assets it made
bool ReimportEditorAsset(EditorSubsystem& subsystem, const Handle<AssetObject>& asset);

static inline bool IsEditorImportModelFile(const FilePath& filepath)
{
    static const char* const modelExtensions[] = { "obj", "fbx", "gltf", "glb" };

    const String extension = filepath.GetExtension().ToLower();

    for (const char* modelExtension : modelExtensions)
    {
        if (extension == modelExtension)
        {
            return true;
        }
    }

    return String(filepath.Basename()).ToLower().EndsWith(".mesh.xml");
}

static inline EnumFlags<AssetLoadHint> GetEditorImportLoadHint(const FilePath& filepath)
{
    static const char* const textureExtensions[] = { "jpg", "jpeg", "png", "tga", "bmp" };
    static const char* const srgbNameTokens[] = { "albedo", "diffuse", "base" };

    const String extension = filepath.GetExtension().ToLower();

    bool isTexture = false;

    for (const char* textureExtension : textureExtensions)
    {
        if (extension == textureExtension)
        {
            isTexture = true;

            break;
        }
    }

    if (!isTexture)
    {
        static const char* const modelExtensions[] = { "obj", "fbx", "gltf", "glb", "xml" };

        for (const char* modelExtension : modelExtensions)
        {
            if (extension == modelExtension)
            {
                return AssetLoadHint::GenerateMeshLods;
            }
        }

        return AssetLoadHint::NoHint;
    }

    const String fileName = String(StringUtil::StripExtension(filepath.Basename())).ToLower();

    for (const char* srgbNameToken : srgbNameTokens)
    {
        if (fileName.Contains(srgbNameToken))
        {
            return AssetLoadHint::TextureSRGB;
        }
    }

    return AssetLoadHint::NoHint;
}

} // namespace Hyperion
