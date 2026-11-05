/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetLoader.hpp>
#include <Asset/Loader.hpp>

#include <Core/FileSystem/FilePath.hpp>
#include <Core/Utilities/Span.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class GLTFModelLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(GLTFModelLoader);

public:
    GLTFModelLoader();
    virtual ~GLTFModelLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;

    ENGINE_API static AssetLoadResult LoadFromMemory(
        ConstByteView glb,
        const FilePath& virtualPath,
        EnumFlags<AssetLoadHint> hint = AssetLoadHint::Transient);
};

} // namespace Hyperion
