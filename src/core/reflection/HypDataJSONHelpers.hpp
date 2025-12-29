/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/Defines.hpp>

namespace Hyperion {

struct BoxedValue;
class Class;

namespace utilities {
struct TypeInfo;
struct TypeId;
} // namespace utilities

using utilities::TypeId;
using utilities::TypeInfo;

namespace Json {

class Value;
class JSObject;

} // namespace Json

struct ToJSONOptions
{
    enum FollowAssetPathsMode
    {
        NEVER = 0,
        WITH_ATTRIBUTE, // with "FollowAssetPath" attribute evaluating to true
        ALWAYS
    };

    bool skipTransientProperties = true;
    bool saveAssetObjectsAsReferences = true;
    bool writeClassNames = false;
    bool writeClassNamesRecursively = false;
    FollowAssetPathsMode followAssetPaths = WITH_ATTRIBUTE;
};

/*! \brief Converts a BoxedValue to a Value.
 *
 *  \param value The BoxedValue to convert.
 *  \param outJson The output Value.
 *  \return True if conversion was successful, false otherwise.
 */
bool HypDataToJSON(
    const BoxedValue& value,
    Json::Value& outJson,
    ToJSONOptions opts = ToJSONOptions {});

/*! \brief Serializes a Object instance to a Object.
 *  Only fields and properties of the Class are serialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members marked with the "transient" attribute are skipped if skipTransientProperties is true.
 *  Members with the "jsonpath" attribute are serialized using the specified JSON path.
 *  \param cls The Class of the object.
 *  \param target The BoxedValue object to serialize.
 *  \param outJson The output Object.
 *  \return True if serialization was successful, false otherwise.
 */
bool ObjectToJSON(
    const Class* cls,
    const BoxedValue& target,
    Json::JSObject& outJson,
    ToJSONOptions opts = ToJSONOptions {});

/*! \brief Converts a Value to BoxedValue
 *
 *  \param jsonValue The Value to convert.
 *  \param typeInfo The TypeInfo for the target type.
 *  \param outHypData The output BoxedValue.
 *  \return True if conversion was successful, false otherwise.
 */
bool JSONToHypData(const Json::Value& jsonValue, const TypeInfo& typeInfo, BoxedValue& outHypData);

/*! \brief Deserializes a Object to a BoxedValue object.
 *  Only fields and properties of the Class are deserialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members with the "jsonpath" attribute are deserialized using the specified JSON path.
 *  \param jsonObject The Object to deserialize.
 *  \param targetClass The desired Class of the target object. (can be null if target already has a type)
 *  \param target The output BoxedValue object.
 *  \return True if deserialization was successful, false otherwise.
 */
bool JSONToObject(const Json::JSObject& jsonObject, const Class* targetClass, BoxedValue& target);

} // namespace Hyperion
