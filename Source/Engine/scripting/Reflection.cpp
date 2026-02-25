/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <scripting/Reflection.hpp>

#include <Core/reflection/ClassRegistry.hpp>
#include <Core/reflection/Class.hpp>
#include <Core/reflection/Field.hpp>

#include <Reflection.generated.inl>

namespace Hyperion {

ClassRef Reflection::GetClassByName(Name name)
{
    return ClassRegistry::GetInstance().GetClass(name);
}

Array<FieldHandle> Reflection::GetFields(const ClassRef& classRef)
{
    Array<FieldHandle> fieldHandles;

    if (!classRef.IsValid())
    {
        return fieldHandles;
    }

    const Class* cls = classRef;

    for (const Field* field : cls->GetFields())
    {
        fieldHandles.PushBack(FieldHandle { field });
    }

    return fieldHandles;
}

FieldHandle Reflection::GetFieldHandle(const ClassRef& classRef, Name fieldName)
{
    if (!classRef.IsValid())
    {
        return FieldHandle {};
    }

    const Class* cls = classRef;

    const Field* field = cls->GetField(fieldName);

    return FieldHandle { field };
}

BoxedValue Reflection::GetFieldValue(const FieldHandle& field, const BoxedValue& target)
{
    if (field.field == nullptr)
    {
        return BoxedValue {};
    }

    return field.field->Get(target);
}

void Reflection::SetFieldValue(const FieldHandle& field, BoxedValue& target, const BoxedValue& value)
{
    if (field.field == nullptr)
    {
        return;
    }

    field.field->Set(target, value);
}

Name Reflection::GetFieldName(const FieldHandle& field)
{
    if (field.field == nullptr)
    {
        return Name();
    }

    return field.field->GetName();
}

Array<MethodHandle> Reflection::GetMethods(const ClassRef& classRef)
{
    Array<MethodHandle> methodHandles;

    if (!classRef.IsValid())
    {
        return methodHandles;
    }

    const Class* cls = classRef;

    for (const Method* method : cls->GetMethods())
    {
        methodHandles.PushBack(MethodHandle { method });
    }

    return methodHandles;
}

MethodHandle Reflection::GetMethodHandle(const ClassRef& classRef, Name methodName)
{
    if (!classRef.IsValid())
    {
        return MethodHandle {};
    }

    const Class* cls = classRef;

    const Method* method = cls->GetMethod(methodName);

    return MethodHandle { method };
}

BoxedValue Reflection::InvokeMethod(const MethodHandle& method, const Array<BoxedValue>& args)
{
    if (method.method == nullptr)
    {
        return BoxedValue {};
    }

    BoxedValue** argsPtrs = args.Any() ? (BoxedValue**)StackAlloc(sizeof(BoxedValue*) * args.Size()) : nullptr;
    if (argsPtrs != nullptr)
    {
        for (uint32 i = 0; i < uint32(args.Size()); i++)
        {
            argsPtrs[i] = const_cast<BoxedValue*>(&args[i]);
        }
    }

    return method.method->Invoke(Span<BoxedValue*>(argsPtrs, args.Size()));
}

Name Reflection::GetMethodName(const MethodHandle& method)
{
    if (method.method == nullptr)
    {
        return Name();
    }

    return method.method->GetName();
}

Array<PropertyHandle> Reflection::GetProperties(const ClassRef& classRef)
{
    Array<PropertyHandle> propertyHandles;

    if (!classRef.IsValid())
    {
        return propertyHandles;
    }

    const Class* cls = classRef;

    for (const Property* property : cls->GetProperties())
    {
        propertyHandles.PushBack(PropertyHandle { property });
    }

    return propertyHandles;
}

PropertyHandle Reflection::GetPropertyHandle(const ClassRef& classRef, Name propertyName)
{
    if (!classRef.IsValid())
    {
        return PropertyHandle {};
    }

    const Class* cls = classRef;

    const Property* property = cls->GetProperty(propertyName);

    return PropertyHandle { property };
}

BoxedValue Reflection::GetPropertyValue(const PropertyHandle& property, const BoxedValue& target)
{
    if (property.property == nullptr)
    {
        return BoxedValue {};
    }

    return property.property->Get(target);
}

void Reflection::SetPropertyValue(const PropertyHandle& property, BoxedValue& target, const BoxedValue& value)
{
    if (property.property == nullptr)
    {
        return;
    }

    property.property->Set(target, value);
}

Name Reflection::GetPropertyName(const PropertyHandle& property)
{
    if (property.property == nullptr)
    {
        return Name();
    }

    return property.property->GetName();
}

} // namespace Hyperion
