/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_CONFIG_HPP
#define HYPERION_FBOM_CONFIG_HPP

#include <core/containers/FlatMap.hpp>

#include <core/utilities/UUID.hpp>

#include <core/serialization/fbom/FBOMObjectLibrary.hpp>

namespace hyperion {

namespace json {
class JSONValue;
} // namespace json

namespace serialization {

class IFBOMConfig
{
public:
    virtual ~IFBOMConfig() = default;

    virtual void SaveToJSON(json::JSONValue& out_json) const = 0;
    virtual bool LoadFromJSON(const json::JSONValue& json) = 0;
};

struct FBOMWriterConfig : public IFBOMConfig
{
    bool enable_static_data = true;
    bool compress_static_data = true;

    virtual ~FBOMWriterConfig() override = default;

    virtual void SaveToJSON(json::JSONValue& out_json) const override;
    virtual bool LoadFromJSON(const json::JSONValue& json) override;
};

struct FBOMReaderConfig : public IFBOMConfig
{
    bool continue_on_external_load_error = false;
    String base_path;

    virtual ~FBOMReaderConfig() override = default;

    virtual void SaveToJSON(json::JSONValue& out_json) const override;
    virtual bool LoadFromJSON(const json::JSONValue& json) override;
};

} // namespace serialization
} // namespace hyperion

#endif
