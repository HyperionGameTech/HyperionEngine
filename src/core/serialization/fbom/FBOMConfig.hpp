/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/Uuid.hpp>

#include <core/serialization/fbom/FBOMObjectLibrary.hpp>

namespace Hyperion {

namespace json {
class JSONValue;
} // namespace json

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

    void SaveToJSON(json::JSONValue& outJson) const;
    bool LoadFromJSON(const json::JSONValue& json);
};

struct FBOMReaderConfig : public FBOMConfig<FBOMReaderConfig>
{
    String basePath;
    bool continueOnExternalLoadError : 1 = false;

    void SaveToJSON(json::JSONValue& outJson) const;
    bool LoadFromJSON(const json::JSONValue& json);
};

} // namespace serialization
} // namespace Hyperion
