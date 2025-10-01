/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/TypeId.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Defines.hpp>

namespace hyperion {

struct HypData;
class HypClass;

namespace json {

class JSONValue;

using JSONString = String;
class JSONObject;

} // namespace json

/*! \brief Converts a JSONValue to a HypData of the specified TypeId.
 *  Supports basic types: int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double, bool, String
 *  Supports Vec2i, Vec3i, Vec4i, Vec2u, Vec3u, Vec4u, Vec2f, Vec3f, Vec4f as arrays of numbers.
 *
 *  \param jsonValue The JSONValue to convert.
 *  \param typeId The TypeId of the target HypData type.
 *  \param outHypData The output HypData.
 *  \return True if conversion was successful, false otherwise.
 */
bool JSONToHypData(const json::JSONValue& jsonValue, TypeId typeId, HypData& outHypData);

/*! \brief Converts a HypData to a JSONValue.
 *  Supports basic types: int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double, bool, String
 *  Supports Vec2i, Vec3i, Vec4i, Vec2u, Vec3u, Vec4u, Vec2f, Vec3f, Vec4f as arrays of numbers.
 *
 *  \param value The HypData to convert.
 *  \param outJson The output JSONValue.
 *  \param skipTransientProperties If true, properties marked as transient will be skipped during serialization. Default is true.
 *  \return True if conversion was successful, false otherwise.
 */
bool HypDataToJSON(const HypData& value, json::JSONValue& outJson, bool skipTransientProperties = true);

/*! \brief Serializes a HypData object to a JSONObject.
 *  Only fields and properties of the HypClass are serialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members marked with the "transient" attribute are skipped if skipTransientProperties is true.
 *  Members with the "jsonpath" attribute are serialized using the specified JSON path.
 *  \param hypClass The HypClass of the object.
 *  \param target The HypData object to serialize.
 *  \param outJson The output JSONObject.
 *  \param skipTransientProperties If true, properties marked as transient will be skipped during serialization
 * .  Default is true.
 *  \return True if serialization was successful, false otherwise.
 */
bool ObjectToJSON(const HypClass* hypClass, const HypData& target, json::JSONObject& outJson, bool skipTransientProperties = true);

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
