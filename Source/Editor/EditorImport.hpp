/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/Loader.hpp>

#include <Core/FileSystem/FilePath.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Utilities/StringUtil.hpp>

namespace Hyperion {

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
