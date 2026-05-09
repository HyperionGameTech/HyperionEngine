/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/String.hpp>
#include <Core/containers/Map.hpp>

#include <Core/utilities/Result.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

struct BoxedValue;
class Class;

namespace utilities {
struct TypeInfo;
struct TypeId;
} // namespace utilities

using utilities::TypeId;
using utilities::TypeInfo;

namespace JSON {

class Value;
class Object;

} // namespace JSON

namespace functional {

template <class FunctionSignature>
class ProcRef;

} // namespace functional

using functional::ProcRef;

struct ToJSONOptions
{
    enum class FollowAssetPathsMode : int
    {
        Never = -1,
        MatchingAttribute = 0, // with "FollowAssetPath" attribute evaluating to true
        Always
    };

    enum class SaveAssetsAsReferencesMode : int
    {
        No,
        UnlessOtherwiseSpecified,
        Yes
    };

    bool skipTransientProperties = true;
    bool writeClassNames = false;
    bool writeClassNamesRecursively = false;

    SaveAssetsAsReferencesMode saveAssetsAsReferences = SaveAssetsAsReferencesMode::UnlessOtherwiseSpecified;
    FollowAssetPathsMode followAssetPaths = FollowAssetPathsMode::MatchingAttribute;
};

/*! \brief Converts a BoxedValue to a Value.
 *
 *  \param value The BoxedValue to convert.
 *  \param outJson The output Value.
 *  \return True if conversion was successful, false otherwise.
 */
Result BoxedToJSON(
    const BoxedValue& value,
    JSON::Value& outJson,
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
Result ObjectToJSON(
    const Class* cls,
    const BoxedValue& target,
    JSON::Object& outJson,
    ToJSONOptions opts = ToJSONOptions {});

/*! \brief Converts a JSON value to BoxedValue
 *
 *  \param jsonValue The Value to convert.
 *  \param typeInfo The TypeInfo for the target type.
 *  \param outBoxed The output BoxedValue.
 *  \return True if conversion was successful, false otherwise.
 */
Result BoxedFromJSON(const JSON::Value& jsonValue, const TypeInfo& typeInfo, BoxedValue& outBoxed);

/*! \brief Deserializes a Object to a BoxedValue object.
 *  Only fields and properties of the Class are deserialized.
 *  Members marked with the "jsonignore" attribute are skipped.
 *  Members with the "jsonpath" attribute are deserialized using the specified JSON path.
 *  \param jsonObject The Object to deserialize.
 *  \param targetClass The desired Class of the target object. (can be null if target already has a type)
 *  \param target The output BoxedValue object.
 *  \return True if deserialization was successful, false otherwise.
 */
Result ObjectFromJSON(const JSON::Object& jsonObject, const Class* targetClass, BoxedValue& target);

void WalkBoxedValue(
    const BoxedValue& target,
    const ProcRef<void(const BoxedValue& current)>& func);

} // namespace Hyperion
