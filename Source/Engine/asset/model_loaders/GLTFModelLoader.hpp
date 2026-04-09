/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetLoader.hpp>

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
};

} // namespace Hyperion
