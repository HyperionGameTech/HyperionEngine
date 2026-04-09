/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetLoader.hpp>

#include <ui/font/FontFace.hpp>
#include <ui/font/FontAtlas.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class FontAtlasLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(FontAtlasLoader);

public:
    virtual ~FontAtlasLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
