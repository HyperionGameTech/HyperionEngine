/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/utilities/Uuid.hpp>

#include <Core/serialization/fbom/FBOMObjectLibrary.hpp>

namespace Hyperion {

namespace JSON {
class Value;
} // namespace JSON

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

    void SaveToJSON(JSON::Value& outJson) const;
    bool LoadFromJSON(const JSON::Value& json);
};

struct FBOMReaderConfig : public FBOMConfig<FBOMReaderConfig>
{
    String basePath;
    bool continueOnExternalLoadError : 1 = false;

    void SaveToJSON(JSON::Value& outJson) const;
    bool LoadFromJSON(const JSON::Value& json);
};

} // namespace serialization
} // namespace Hyperion
