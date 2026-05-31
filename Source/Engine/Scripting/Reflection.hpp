/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/Class.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

class Field;
class Method;
class Property;

HYP_STRUCT(OnlyLanguages = "hypscript")
struct FieldHandle
{
    HYP_STRUCT_BODY(FieldHandle)

    const Field* field = nullptr;
};

HYP_STRUCT(OnlyLanguages = "hypscript")
struct MethodHandle
{
    HYP_STRUCT_BODY(MethodHandle)

    const Method* method = nullptr;
};

HYP_STRUCT(OnlyLanguages = "hypscript")
struct PropertyHandle
{
    HYP_STRUCT_BODY(PropertyHandle)

    const Property* property = nullptr;
};

/// Script utility for interacting with the reflection system from scripts. Not intended for use in native code.
HYP_CLASS(OnlyLanguages = "hypscript")
class ENGINE_API Reflection final : public ObjectBase
{
    HYP_OBJECT_BODY(Reflection);

public:
    HYP_METHOD()
    static ClassRef GetClassByName(Name name);

    /// Fields

    HYP_METHOD()
    static Array<FieldHandle> GetFields(const ClassRef& classRef);

    HYP_METHOD()
    static FieldHandle GetFieldHandle(const ClassRef& classRef, Name fieldName);

    HYP_METHOD()
    static BoxedValue GetFieldValue(const FieldHandle& field, const BoxedValue& target);

    HYP_METHOD()
    static void SetFieldValue(const FieldHandle& field, BoxedValue& target, const BoxedValue& value);

    HYP_METHOD()
    static Name GetFieldName(const FieldHandle& field);

    /// Methods

    HYP_METHOD()
    static Array<MethodHandle> GetMethods(const ClassRef& classRef);

    HYP_METHOD()
    static MethodHandle GetMethodHandle(const ClassRef& classRef, Name methodName);

    HYP_METHOD()
    static BoxedValue InvokeMethod(const MethodHandle& method, const Array<BoxedValue>& args);

    HYP_METHOD()
    static Name GetMethodName(const MethodHandle& method);

    /// Properties

    HYP_METHOD()
    static Array<PropertyHandle> GetProperties(const ClassRef& classRef);

    HYP_METHOD()
    static PropertyHandle GetPropertyHandle(const ClassRef& classRef, Name propertyName);

    HYP_METHOD()
    static BoxedValue GetPropertyValue(const PropertyHandle& property, const BoxedValue& target);

    HYP_METHOD()
    static void SetPropertyValue(const PropertyHandle& property, BoxedValue& target, const BoxedValue& value);

    HYP_METHOD()
    static Name GetPropertyName(const PropertyHandle& property);
};

} // namespace Hyperion
