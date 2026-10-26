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

void ImportEditorContent(
    EditorSubsystem* subsystem,
    const Array<FilePath>& files,
    const String& targetFolder,
    EditorImportCallback&& onImported = {},
    bool reviewModels = false);

Array<Handle<AssetObject>> CommitEditorModelImport(
    EditorSubsystem& subsystem,
    const ModelImportContents& contents,
    const AssetImportInfo& importInfo,
    const String& targetFolder);

const AssetImportInfo* GetEditorAssetImportInfo(const AssetObject* asset);

bool ReimportEditorAsset(EditorSubsystem& subsystem, const Handle<AssetObject>& asset);

// @TODO Clean up

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

// @TODO Clean up

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
