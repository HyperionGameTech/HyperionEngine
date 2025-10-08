/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Defines.hpp>

namespace hyperion {

struct HypData;
class HypClass;

namespace utilities {
struct TypeInfo;
struct TypeId;
} // namespace utilities

using utilities::TypeId;
using utilities::TypeInfo;

namespace json {

class JSONValue;

using JSONString = String;
class JSONObject;

} // namespace json

struct ToJSONOptions
{
    bool skipTransientProperties = true;
    bool saveAssetObjectsAsReferences = true;
    bool writeClassNames = false;
    bool writeClassNamesRecursively = false;
};

/*! \brief Converts a HypData to a JSONValue.
 *
 *  \param value The HypData to convert.
 *  \param outJson The output JSONValue.
 *  \return True if conversion was successful, false otherwise.
 */
bool HypDataToJSON(
    const HypData& value,
    json::JSONValue& outJson,
    ToJSONOptions opts = ToJSONOptions {});

/*! \brief Serializes a HypObject instance to a JSONObject.
 *  Only fields and properties of the HypClass are serialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members marked with the "transient" attribute are skipped if skipTransientProperties is true.
 *  Members with the "jsonpath" attribute are serialized using the specified JSON path.
 *  \param hypClass The HypClass of the object.
 *  \param target The HypData object to serialize.
 *  \param outJson The output JSONObject.
 *  \return True if serialization was successful, false otherwise.
 */
bool ObjectToJSON(
    const HypClass* hypClass,
    const HypData& target,
    json::JSONObject& outJson,
    ToJSONOptions opts = ToJSONOptions {});

/*! \brief Converts a JSONValue to HypData
 *
 *  \param jsonValue The JSONValue to convert.
 *  \param typeInfo The TypeInfo for the target type.
 *  \param outHypData The output HypData.
 *  \return True if conversion was successful, false otherwise.
 */
bool JSONToHypData(const json::JSONValue& jsonValue, const TypeInfo& typeInfo, HypData& outHypData);

/*! \brief Deserializes a JSONObject to a HypData object.
 *  Only fields and properties of the HypClass are deserialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members with the "jsonpath" attribute are deserialized using the specified JSON path.
 *  \param jsonObject The JSONObject to deserialize.
 *  \param hypClass The HypClass of the object.
 *  \param target The output HypData object.
 *  \return True if deserialization was successful, false otherwise.
 */
bool JSONToObject(const json::JSONObject& jsonObject, const HypClass* hypClass, HypData& target);

} // namespace hyperion
