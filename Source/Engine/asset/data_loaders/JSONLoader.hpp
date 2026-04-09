/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/Assets.hpp>
#include <Core/json/JSON.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

using namespace JSON;

HYP_CLASS(NoScriptBindings)
class JSONLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(JSONLoader);

public:
    virtual ~JSONLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
