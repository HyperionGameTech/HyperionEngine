/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_JSON_LOADER_HPP
#define HYPERION_JSON_LOADER_HPP

#include <asset/Assets.hpp>
#include <core/json/JSON.hpp>

#include <Types.hpp>

namespace hyperion {

using namespace json;

HYP_CLASS(NoScriptBindings)
class JSONLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(JSONLoader);
    
public:
    virtual ~JSONLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

#endif