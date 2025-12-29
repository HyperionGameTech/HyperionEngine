/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/Uuid.hpp>

#include <core/serialization/fbom/FBOMObjectLibrary.hpp>

namespace Hyperion {

namespace Json {
class Value;
} // namespace Json

namespace serialization {

template <class Derived>
struct FBOMConfig
{
};

/// \todo Convert these structs to use Configuration system (see core/config/Config.hpp)

struct FBOMWriterConfig : public FBOMConfig<FBOMWriterConfig>
{
    bool enableStaticData : 1 = true;
    bool compressStaticData : 1 = true;

    void SaveToJSON(Json::Value& outJson) const;
    bool LoadFromJSON(const Json::Value& json);
};

struct FBOMReaderConfig : public FBOMConfig<FBOMReaderConfig>
{
    String basePath;
    bool continueOnExternalLoadError : 1 = false;

    void SaveToJSON(Json::Value& outJson) const;
    bool LoadFromJSON(const Json::Value& json);
};

} // namespace serialization
} // namespace Hyperion
