/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetLoader.hpp>

#include <UI/Font/FontFace.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class FontFaceLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(FontFaceLoader);

public:
    virtual ~FontFaceLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
