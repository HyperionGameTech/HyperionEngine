/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HYP_DATA_JSON_HELPERS_HPP
#define HYPERION_HYP_DATA_JSON_HELPERS_HPP

#include <core/utilities/TypeID.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Defines.hpp>

namespace hyperion {

struct HypData;
class HypClass;

namespace json {

class JSONValue;

using JSONString = String;
using JSONObject = HashMap<JSONString, JSONValue>;

} // namespace json

bool JSONToHypData(const json::JSONValue &json_value, TypeID type_id, HypData &out_hyp_data);
bool HypDataToJSON(const HypData &value, json::JSONValue &out_json);
bool ObjectToJSON(const HypClass *hyp_class, const HypData &target, json::JSONObject &out_json);
bool JSONToObject(const json::JSONObject &json_object, const HypClass *hyp_class, HypData &target);

} // namespace hyperion

#endif