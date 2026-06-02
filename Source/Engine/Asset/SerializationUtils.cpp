/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Asset/SerializationUtils.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetReference.hpp>

#include <Core/JSON/JSON.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/StaticField.hpp>
#include <Core/Reflection/Method.hpp>
#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FlatSet.hpp>
#include <Core/Containers/List.hpp>

#include <Core/Utilities/Format.hpp>
#include <Core/Utilities/Uuid.hpp>
#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/Float16.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

namespace Hyperion {

struct LoadAssetsFromReferencesContext
{
};

// used to prevent infinite recursion when serializing nested objects
static thread_local TSet<Pair<TypeId, const void*>> s_serializedObjects;

// If true, class names will always be written when serializing objects IF the type != the declared type.
// For example, we're serializing an array of Animal and we encounter a Dog object, we need to write the class name
// otherwise we won't know to deserialize it as a Dog.
static constexpr bool ForceWriteClassNamesWhenTypesDiffer = true;

Result BoxedToJSON(
    const BoxedValue& value,
    JSON::Value& outJson,
    ToJSONOptions* pOptions)
{
    static ToJSONOptions s_defaultOptions;
    if (!pOptions)
    {
        pOptions = &s_defaultOptions;
    }

    ToJSONOptions& opts = *pOptions;

    if (opts.writeClassNamesRecursively)
    {
        opts.writeClassNames = true;
    }

    if (value.IsNull())
    {
        outJson = JSON::JSNull();

        return {};
    }

    const TypeInfo& typeInfo = *value.GetTypeInfo();

    AssetReference assetReference;
    bool isAssetObject = false;

    if (value.Is<AssetReference>())
    {
        assetReference = value.Get<AssetReference>();
    }
    else if (value.Is<AssetPath>() && opts.followAssetPaths >= ToJSONOptions::FollowAssetPathsMode::MatchingAttribute)
    {
        assetReference = AssetReference(value.Get<AssetPath>());
    }
    else if (value.Is<AssetObject>())
    {
        // Serialize AssetObject deriving classes by their asset reference if we're saving an Editor project.
        const AssetObject& assetObject = value.Get<AssetObject>();
        // AssertDebug(assetObject.IsRegistered(), "Cannot serialize unregistered AssetObject {}", assetObject.GetName());

        // if (!assetObject.IsRegistered())
        //{
        //     return HYP_MAKE_ERROR(Error, "Cannot serialize unregistered AssetObject");
        // }

        assetReference = AssetReference(assetObject.HandleFromThis());
        isAssetObject = true;
    }

    if (isAssetObject) // && opts.saveAssetsAsReferences >= ToJSONOptions::SaveAssetsAsReferencesMode::UnlessOtherwiseSpecified)
    {
        AssertDebug(assetReference.IsValid(), "Serializing invalid asset reference");

        ToJSONOptions newOpts = opts;
        newOpts.writeClassNames |= ForceWriteClassNamesWhenTypesDiffer;

        return BoxedToJSON(BoxedValue(assetReference), outJson, &newOpts);
    }

    assetReference = AssetReference(); // reset

    if (value.Is<bool>(/* strict */ true))
    {
        outJson = value.Get<bool>();

        return {};
    }

#define DO_NUMERIC_TYPE(T)                      \
    if (value.Is<T>(/* strict */ true))         \
    {                                           \
        outJson = JSON::Number(value.Get<T>()); \
                                                \
        return {};                              \
    }

    DO_NUMERIC_TYPE(int8);
    DO_NUMERIC_TYPE(int16);
    DO_NUMERIC_TYPE(int32);
    DO_NUMERIC_TYPE(int64);

    DO_NUMERIC_TYPE(uint8);
    DO_NUMERIC_TYPE(uint16);
    DO_NUMERIC_TYPE(uint32);
    DO_NUMERIC_TYPE(uint64);

    DO_NUMERIC_TYPE(Float16);
    DO_NUMERIC_TYPE(float);
    DO_NUMERIC_TYPE(double);

#ifdef HYP_WINDOWS
    if constexpr (!std::is_same_v<size_t, uint64> && !std::is_same_v<size_t, uint32>)
    {
        DO_NUMERIC_TYPE(size_t);
    }
#endif

#undef DO_NUMERIC_TYPE

    if (value.Is<String>())
    {
        outJson = JSON::JString(value.Get<String>());

        return {};
    }

    if (typeInfo.IsVectorType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_VECTOR);

        ITypeInfoVectorHandler* handler = static_cast<ITypeInfoVectorHandler*>(typeInfo.extendedInfo.handler);

        const int size = handler->GetNumComponents();

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        for (int i = 0; i < size; i++)
        {
            const AnyRef element = handler->GetComponent(value, i);

            if (!element.HasValue())
            {
                HYP_LOG(Core, Warning, "Failed to get element at index {} of vector of type {}", i, typeInfo.name);

                continue;
            }

            JSON::Value jsonValue;
            if (Result result = BoxedToJSON(BoxedValue(element), jsonValue, &opts); result.HasError())
            {
                return result;
            }

            jsonArray.PushBack(std::move(jsonValue));
        }

        outJson = std::move(jsonArray);

        return {};
    }

    if (typeInfo.IsMatrixType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_MATRIX);

        ITypeInfoMatrixHandler* handler = static_cast<ITypeInfoMatrixHandler*>(typeInfo.extendedInfo.handler);

        const int rows = handler->GetNumRows();
        const int columns = handler->GetNumColumns();

        JSON::JArray jsonArray;
        jsonArray.Resize(rows * columns);

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < columns; c++)
            {
                const AnyRef element = handler->GetElement(value, r, c);

                if (!element.HasValue())
                {
                    return HYP_MAKE_ERROR(Error, "Failed to get element at ({}, {}) of matrix of type {}", r, c, typeInfo.name);
                }

                JSON::Value jsonValue;
                if (Result result = BoxedToJSON(BoxedValue(element), jsonValue, &opts); result.HasError())
                {
                    return result;
                }

                jsonArray[r * columns + c] = std::move(jsonValue);
            }
        }

        outJson = std::move(jsonArray);

        return {};
    }

    if (value.Is<UUID>())
    {
        const UUID& uuid = value.Get<UUID>();

        outJson = JSON::JString(uuid.ToString());

        return {};
    }

    if (value.Is<Name>())
    {
        const Name name = value.Get<Name>();

        outJson = JSON::JString(name.LookupString());

        return {};
    }

    if (typeInfo.IsArrayType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_ARRAY);

        ITypeInfoArrayHandler* handler = static_cast<ITypeInfoArrayHandler*>(typeInfo.extendedInfo.handler);

        const size_t size = handler->GetSize(value);

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        for (size_t i = 0; i < size; i++)
        {
            BoxedValue element;

            if (!handler->GetElementAt(value, i, element))
            {
                HYP_LOG(Core, Warning, "Failed to get element at index {} of array of type {}", i, typeInfo.name);

                continue;
            }

            ToJSONOptions newOpts = opts;
            newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && typeInfo.extendedInfo.GetElementType()->GetClass() != element.GetTypeInfo()->GetClass())
            {
                newOpts.writeClassNames = true;
            }

            if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
            {
                newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
            }

            /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
            {
                newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
            }*/

            JSON::Value jsonValue;
            if (Result result = BoxedToJSON(element, jsonValue, &newOpts); result.HasError())
            {
                return result;
            }

            jsonArray.PushBack(std::move(jsonValue));
        }

        outJson = std::move(jsonArray);

        return {};
    }

    if (typeInfo.IsSetType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_SET);

        ITypeInfoSetHandler* handler = static_cast<ITypeInfoSetHandler*>(typeInfo.extendedInfo.handler);

        const size_t size = handler->GetSize(value);

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        // Use iterator to traverse set elements
        ITypeInfoIterator* iterator = handler->CreateIterator(value);
        HYP_DEFER({ delete iterator; });

        if (!iterator)
        {
            return HYP_MAKE_ERROR(Error, "Failed to create iterator for set of type {}", typeInfo.name);
        }

        while (iterator->HasNext())
        {
            AnyRef element = iterator->GetCurrent();

            if (!element.HasValue())
            {
                HYP_LOG(Core, Warning, "Failed to get current element from set iterator of type {}", typeInfo.name);
                iterator->Next();
                continue;
            }

            ToJSONOptions newOpts = opts;
            newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

            BoxedValue elementData(element);

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && typeInfo.extendedInfo.GetElementType()->GetClass() != elementData.GetTypeInfo()->GetClass())
            {
                newOpts.writeClassNames = true;
            }

            if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
            {
                newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
            }

            /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
            {
                newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
            }*/

            JSON::Value jsonValue;
            if (Result result = BoxedToJSON(elementData, jsonValue, &newOpts); result.HasError())
            {
                return result;
            }

            jsonArray.PushBack(std::move(jsonValue));

            iterator->Next();
        }

        outJson = std::move(jsonArray);

        return {};
    }

    if (typeInfo.IsMapType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_MAP);

        ITypeInfoMapHandler* handler = static_cast<ITypeInfoMapHandler*>(typeInfo.extendedInfo.handler);

        const TypeInfo* keyTypeInfo = typeInfo.extendedInfo.GetElementType();
        const TypeInfo* valueTypeInfo = typeInfo.extendedInfo.next ? typeInfo.extendedInfo.next->GetElementType() : nullptr;

        Assert(keyTypeInfo != nullptr);
        Assert(valueTypeInfo != nullptr);

        ITypeInfoIterator* iterator = handler->CreateIterator(value);
        HYP_DEFER({ delete iterator; });

        if (!iterator)
        {
            return HYP_MAKE_ERROR(Error, "Failed to create iterator for map of type {}", typeInfo.name);
        }

        if (keyTypeInfo->IsStringType())
        {
            JSON::Object jsonObject;

            while (iterator->HasNext())
            {
                AnyRef keyRef = iterator->GetKey();
                AnyRef valueRef = iterator->GetCurrent();

                if (!keyRef.HasValue() || !valueRef.HasValue())
                {
                    HYP_LOG(Core, Warning, "Failed to get key/value from map iterator of type {}", typeInfo.name);
                    iterator->Next();
                    continue;
                }

                BoxedValue keyData(keyRef);
                BoxedValue valueData(valueRef);

                JSON::Value keyJson;
                if (Result result = BoxedToJSON(keyData, keyJson, &opts); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to serialize map key for type {}: {}", typeInfo.name, result.GetError().GetMessage());
                    iterator->Next();
                    continue;
                }

                if (!keyJson.IsString())
                {
                    HYP_LOG(Core, Warning, "Map key for type {} did not serialize to a JSON string", typeInfo.name);
                    iterator->Next();
                    continue;
                }

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && valueTypeInfo->GetClass() != valueData.GetTypeInfo()->GetClass())
                {
                    newOpts.writeClassNames = true;
                }

                if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                JSON::Value jsonValue;
                if (Result result = BoxedToJSON(valueData, jsonValue, &newOpts); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to serialize map value for key \"{}\" of type {}: {}", keyJson.AsString(), typeInfo.name, result.GetError().GetMessage());
                    iterator->Next();
                    continue;
                }

                jsonObject[keyJson.AsString()] = std::move(jsonValue);

                iterator->Next();
            }

            outJson = std::move(jsonObject);
        }
        else
        {
            // Non-string keys: serialize as an array of [key, value] pairs
            JSON::JArray jsonArray;

            while (iterator->HasNext())
            {
                AnyRef keyRef = iterator->GetKey();
                AnyRef valueRef = iterator->GetCurrent();

                if (!keyRef.HasValue() || !valueRef.HasValue())
                {
                    HYP_LOG(Core, Warning, "Failed to get key/value from map iterator of type {}", typeInfo.name);
                    iterator->Next();
                    continue;
                }

                BoxedValue keyData(keyRef);
                BoxedValue valueData(valueRef);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && valueTypeInfo->GetClass() != valueData.GetTypeInfo()->GetClass())
                {
                    newOpts.writeClassNames = true;
                }

                if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                JSON::JArray pairArray;
                pairArray.Resize(2);

                JSON::Value keyJson;
                if (Result result = BoxedToJSON(keyData, keyJson, &newOpts); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to serialize map key for type {}: {}", typeInfo.name, result.GetError().GetMessage());
                    iterator->Next();
                    continue;
                }

                pairArray[0] = std::move(keyJson);

                JSON::Value valueJson;
                if (Result result = BoxedToJSON(valueData, valueJson, &newOpts); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to serialize map value for type {}: {}", typeInfo.name, result.GetError().GetMessage());
                    iterator->Next();
                    continue;
                }

                pairArray[1] = std::move(valueJson);

                jsonArray.PushBack(std::move(pairArray));

                iterator->Next();
            }

            outJson = std::move(jsonArray);
        }

        return {};
    }

    if (typeInfo.IsEnum() || typeInfo.IsEnumFlags())
    {
        const TypeInfo* underlyingTypeInfo = typeInfo.GetUnderlyingType();
        AssertDebug(underlyingTypeInfo != nullptr, "Enum type must have an underlying type");

        constexpr int64 MaxDblValue = std::bit_cast<int64>(DBL_MAX);
        constexpr uint64 MaxDblValueUnsigned = std::bit_cast<uint64>(DBL_MAX);

        if (underlyingTypeInfo->id == TypeId::ForType<uint8>()
            || underlyingTypeInfo->id == TypeId::ForType<uint16>()
            || underlyingTypeInfo->id == TypeId::ForType<uint32>()
            || underlyingTypeInfo->id == TypeId::ForType<uint64>())
        {

            if (value.Get<uint64>() > MaxDblValueUnsigned)
            {
                HYP_LOG(Core, Warning, "Enum value {} of type {} is too large to be represented as a JSON number without loss of precision", value.Get<uint64>(), typeInfo.name);
            }

            outJson = JSON::Number(value.Get<uint64>());

            return {};
        }
        else if (underlyingTypeInfo->id == TypeId::ForType<int8>()
            || underlyingTypeInfo->id == TypeId::ForType<int16>()
            || underlyingTypeInfo->id == TypeId::ForType<int32>()
            || underlyingTypeInfo->id == TypeId::ForType<int64>())
        {
            if (value.Get<int64>() > MaxDblValue || value.Get<int64>() < -MaxDblValue)
            {
                HYP_LOG(Core, Warning, "Enum value {} of type {} is too large to be represented as a JSON number without loss of precision", value.Get<int64>(), typeInfo.name);
            }

            outJson = JSON::Number(value.Get<int64>());

            return {};
        }

        return HYP_MAKE_ERROR(Error, "Enum type {} has unsupported underlying type {}", typeInfo.name, underlyingTypeInfo->name);
    }

    if (typeInfo.IsPairType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_PAIR);

        ITypeInfoPairHandler* pairHandler = static_cast<ITypeInfoPairHandler*>(typeInfo.extendedInfo.handler);

        const TypeInfo* firstType = typeInfo.extendedInfo.GetElementType();
        Assert(firstType != nullptr);

        const TypeInfo* secondType = typeInfo.extendedInfo.next ? typeInfo.extendedInfo.next->GetElementType() : nullptr;
        Assert(secondType != nullptr);

        // we store pair types as array of 2 elems
        JSON::JArray jsonArray;
        jsonArray.Resize(2);

        {
            BoxedValue firstBoxed;
            pairHandler->GetFirst(value, firstBoxed);

            ToJSONOptions newOpts = opts;
            newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && firstType->GetClass() != firstBoxed.GetTypeInfo()->GetClass())
            {
                newOpts.writeClassNames = true;
            }

            if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
            {
                newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
            }

            /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
            {
                newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
            }*/

            if (Result result = BoxedToJSON(firstBoxed, jsonArray[0], &newOpts); result.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to serialize first element of pair for type {}: {}", typeInfo.name, result.GetError().GetMessage());
            }
        }

        {
            BoxedValue secondBoxed;
            pairHandler->GetSecond(value, secondBoxed);

            ToJSONOptions newOpts = opts;
            newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

            if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && secondType->GetClass() != secondBoxed.GetTypeInfo()->GetClass())
            {
                newOpts.writeClassNames = true;
            }

            if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
            {
                newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
            }

            /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
            {
                newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
            }*/

            if (Result result = BoxedToJSON(secondBoxed, jsonArray[1], &newOpts); result.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to serialize second element of pair for type {}: {}", typeInfo.name, result.GetError().GetMessage());
            }
        }

        outJson = std::move(jsonArray);

        return {};
    }

    if (typeInfo.IsVariantType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_VARIANT);

        ITypeInfoVariantHandler* handler = static_cast<ITypeInfoVariantHandler*>(typeInfo.extendedInfo.handler);

        if (handler->GetCurrentTypeIndex(value) == Variant<std::nullptr_t>::invalidTypeIndex)
        {
            // no value set, fine
            outJson = JSON::JSNull();

            return {};
        }

        AnyRef activeValue = handler->GetValue(value);

        if (!activeValue.HasValue())
        {
            return HYP_MAKE_ERROR(Error, "Failed to get active value of variant of type {}", typeInfo.name);
        }

        ToJSONOptions newOpts = opts;
        newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

        // always (if ForceWriteClassNamesWhenTypesDiffer is true) write out class names for variants.
        if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer)
        {
            newOpts.writeClassNames = true;
        }

        if (newOpts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always)
        {
            newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
        }

        /*if (newOpts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes)
        {
            newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
        }*/

        return BoxedToJSON(BoxedValue(activeValue), outJson, &newOpts);
    }

    const Class* cls = GetClass(value.GetTypeId());

    if (cls)
    {
        Pair<TypeId, const void*> pair = { value.GetTypeId(), value.ToRef().GetPointer() };

        if (!s_serializedObjects.Insert(pair).second)
        {
            return HYP_MAKE_ERROR(Error, "Detected circular reference when serializing BoxedValue to JSON");
        }

        HYP_DEFER({
            s_serializedObjects.Erase(pair);
        });

        JSON::Object jsonObject;

        if (Result result = ObjectToJSON(cls, value, jsonObject, &opts); result.HasError())
        {
            return result;
        }

        outJson = std::move(jsonObject);

        return {};
    }

    return HYP_MAKE_ERROR(Error, "Don't know how to serialize BoxedValue with type \"{}\" to JSON", typeInfo.name);
}

Result ObjectToJSON(const Class* cls, const BoxedValue& target, JSON::Object& outJson, ToJSONOptions* pOptions)
{
    static ToJSONOptions s_defaultOptions;
    if (!pOptions)
    {
        pOptions = &s_defaultOptions;
    }

    ToJSONOptions& opts = *pOptions;

    if (opts.writeClassNamesRecursively)
    {
        opts.writeClassNames = true;
    }

    TSet<Name> usedMembers;

    const Class* originalClass = cls;

    while (cls != nullptr)
    {
        // look for signature void ToJSON(Value& out)
        if (const Method* toJsonMethod = cls->GetMethod("ToJSON"_sh, /* deep */ false))
        {
            if (toJsonMethod->GetParameters().Size() != 2
                || toJsonMethod->GetTypeId() != TypeId::Void()
                || toJsonMethod->GetParameters()[0].typeInfo->id != cls->GetTypeId()
                || toJsonMethod->GetParameters()[1].typeInfo->id != TypeId::ForType<JSON::Value>())
            {
                HYP_LOG(Core, Warning, "Class \"{}\" has a ToJSON method but it has an invalid signature", cls->GetName());
            }
            else
            {
                BoxedValue returnValue;
                toJsonMethod->Invoke(Array<BoxedValue*> { const_cast<BoxedValue*>(&target), &returnValue });

                if (returnValue.Is<JSON::Value>())
                {
                    JSON::Value& jsonValue = returnValue.Get<JSON::Value>();

                    if (jsonValue.IsObject())
                    {
                        JSON::Object& jsonObject = jsonValue.AsObject();
                        std::swap(outJson, jsonObject);
                        outJson.MergeDeep(jsonObject);

                        // skip normal serialization since we got the full object from ToJSON
                        continue;
                    }
                    else
                    {
                        HYP_LOG(Core, Warning, "ToJSON method of Class \"{}\" did not return a valid JSON object, got JSON value: {}", cls->GetName(), jsonValue.ToString());
                    }
                }
                else
                {
                    HYP_LOG(Core, Warning, "Failed to invoke ToJSON method of Class \"{}\", got type {} but expected Value!",
                        cls->GetName(), returnValue.GetTypeInfo()->name);
                }
            }
        }

        for (const IMember& member : cls->GetMembers(MemberType::Field | MemberType::Property, /* deep */ false))
        {
            if (opts.skipTransientProperties)
            {
                if (const ClassAttributeValue& attribute = member.GetAttribute(Attributes::g_attrTransient); attribute.IsValid() && attribute.GetBool())
                {
                    continue;
                }
            }

            if (member.GetMemberType() != MemberType::Property && member.GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                //  skip fields that are marked as properties otherwise they will be serialized twice
                continue;
            }

            if (member.IsDelegate())
            {
                // skip delegates
                continue;
            }

            if (const ClassAttributeValue& attribute = member.GetAttribute(Attributes::g_attrJsonIgnore); attribute.IsValid() && attribute.GetBool())
            {
                continue;
            }

            if (usedMembers.Contains(member.GetName()))
            {
                continue;
            }

            usedMembers.Insert(member.GetName());

            switch (member.GetMemberType())
            {
            case MemberType::Property:
            {
                const Property* property = static_cast<const Property*>(&member);

                BoxedValue value = property->Get(target);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && property->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                if (opts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always && !property->GetAttribute(Attributes::g_attrFollowAssetPath).GetBool())
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                /*if (opts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes && !property->GetAttribute(Attributes::g_attrSaveAsReference).GetBool(true))
                {
                    newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
                }*/

                JSON::Value jsonValue;

                if (Result result = BoxedToJSON(value, jsonValue, &newOpts); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to serialize property \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *property->GetName();

                if (const ClassAttributeValue& pathAttribute = property->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    JSON::Value temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    outJson = std::move(temp.AsObject());
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            case MemberType::Field:
            {
                const Field* field = static_cast<const Field*>(&member);

                BoxedValue value = field->Get(target);

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && field->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                if (opts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always && !field->GetAttribute(Attributes::g_attrFollowAssetPath).GetBool())
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                /*if (opts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes && !field->GetAttribute(Attributes::g_attrSaveAsReference).GetBool(true))
                {
                    newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
                }*/

                JSON::Value jsonValue;

                if (Result result = BoxedToJSON(value, jsonValue, &newOpts); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to serialize field \"{}\" of Class \"{}\" to json: {}",
                        member.GetName(), cls->GetName(), result.GetError().GetMessage());

                    continue;
                }

                String path = *field->GetName();

                if (const ClassAttributeValue& pathAttribute = field->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    JSON::Value temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    JSON::Object& obj = temp.AsObject();

                    outJson = std::move(obj);
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            case MemberType::StaticField:
            {
                const StaticField* staticField = static_cast<const StaticField*>(&member);

                BoxedValue value = staticField->Get();

                ToJSONOptions newOpts = opts;
                newOpts.writeClassNames = newOpts.writeClassNamesRecursively;

                if (!newOpts.writeClassNames && ForceWriteClassNamesWhenTypesDiffer && staticField->GetTypeInfo().GetClass() != GetClass(value.GetTypeId()))
                {
                    newOpts.writeClassNames = true;
                }

                if (opts.followAssetPaths != ToJSONOptions::FollowAssetPathsMode::Always && !staticField->GetAttribute(Attributes::g_attrFollowAssetPath).GetBool())
                {
                    newOpts.followAssetPaths = ToJSONOptions::FollowAssetPathsMode::Never;
                }

                /*if (opts.saveAssetsAsReferences != ToJSONOptions::SaveAssetsAsReferencesMode::Yes && !staticField->GetAttribute(Attributes::g_attrSaveAsReference).GetBool(true))
                {
                    newOpts.saveAssetsAsReferences = ToJSONOptions::SaveAssetsAsReferencesMode::No;
                }*/

                JSON::Value jsonValue;

                if (Result result = BoxedToJSON(staticField->Get(), jsonValue, &newOpts); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to serialize static field \"{}\" of Class \"{}\" to json",
                        member.GetName(), cls->GetName());

                    continue;
                }

                String path = *staticField->GetName();

                if (const ClassAttributeValue& pathAttribute = staticField->GetAttribute(Attributes::g_attrJsonPath); pathAttribute.IsValid())
                {
                    path = pathAttribute.GetString();

                    JSON::Value temp(std::move(outJson));
                    temp.Set(path, jsonValue);

                    outJson = std::move(temp.AsObject());
                }
                else
                {
                    outJson[path] = std::move(jsonValue);
                }

                break;
            }
            default:
                break;
            }
        }

        cls = cls->GetParent();
    }

    if (originalClass && opts.writeClassNames)
    {
        auto classIt = outJson.Find("$Class");
        if (classIt != outJson.End())
        {
            outJson.Erase(classIt);
        }

        // write class at the beginning of the object
        outJson.PushFront({ "$Class", *originalClass->GetName() });
    }

    return {};
}

Result ObjectFromJSON(const JSON::Object& jsonObject, const Class* targetClass, BoxedValue& target)
{
    auto ResolveMember = [&target](const IMember& member, const JSON::Value& value) -> bool
    {
        switch (member.GetMemberType())
        {
        case MemberType::Property:
        {
            const Property& property = static_cast<const Property&>(member);

            if (!property.CanSet())
                return false; // true; // skip read-only properties silently.

            const TypeInfo& typeInfo = property.GetTypeInfo();

            BoxedValue boxed;

            if (Result result = BoxedFromJSON(value, typeInfo, boxed); result.HasError())
            {
                HYP_LOG(Core, Warning, "Failed to deserialize property \"{}\" from json: {}",
                    member.GetName(),
                    result.GetError().GetMessage());

                return false;
            }

            if (boxed.IsNull())
            {
                // dont set null values
                return true;
            }

            property.Set(target, boxed);

            return true;
        }
        case MemberType::Field:
        {
            const Field& field = static_cast<const Field&>(member);
            const TypeInfo& typeInfo = field.GetTypeInfo();

            BoxedValue boxed;

            if (Result result = BoxedFromJSON(value, typeInfo, boxed); result.HasError())
            {
                HYP_LOG(Core, Warning, "Failed to deserialize field \"{}\" from json: {}",
                    member.GetName(),
                    result.GetError().GetMessage());

                return false;
            }

            if (boxed.IsNull())
            {
                // dont set null values
                return true;
            }

            field.Set(target, boxed);

            return true;
        }
        default:
            break;
        }

        return false;
    };

    JSON::Value jsonObjectValue { jsonObject };

    const Class* instanceClass = target.GetTypeInfo()->GetClass();

    if (!instanceClass)
    {
        instanceClass = targetClass;
    }

    if (instanceClass != nullptr)
    {
        if (target.IsNull())
        {
            if (!instanceClass->CreateInstance(target, /* allowAbstract */ false))
            {
                return HYP_MAKE_ERROR(Error, "Failed to create instance of Class");
            }
        }

        GlobalContextScope contextScope { LoadAssetsFromReferencesContext() };

        // members with LoadOrder attribute get binned and put here first
        SortedArray<KeyValuePair<int, const IMember*>> sortedMembers;

        // reoslve jsonpath members first
        for (const IMember& member : instanceClass->GetMembers(MemberType::Field | MemberType::Property))
        {
            if (const ClassAttributeValue& attribute = member.GetAttribute(Attributes::g_attrJsonIgnore); attribute.IsValid() && attribute.GetBool())
            {
                continue;
            }

            const ClassAttributeValue& pathAttribute = member.GetAttribute(Attributes::g_attrJsonPath);

            if (!pathAttribute.IsValid())
            {
                continue;
            }

            UTF8StringView path = pathAttribute.GetString();

            auto value = jsonObjectValue.Get(path);

            if (!value.value)
            {
                HYP_LOG(Core, Warning, "Failed to resolve JSON path \"{}\" for Class \"{}\"", path, instanceClass->GetName());

                continue;
            }

            if (const ClassAttributeValue& sortOrderAttribute = member.GetAttribute(Attributes::g_attrLoadOrder); sortOrderAttribute.IsValid())
            {
                sortedMembers.Insert({ sortOrderAttribute.GetInt(), &member });

                continue;
            }

            // not sorted
            sortedMembers.Insert({ 0, &member });
        }

        for (const KeyValuePair<int, const IMember*>& pair : sortedMembers)
        {
            if (!ResolveMember(*pair.second, *jsonObjectValue.Get(pair.second->GetAttribute(Attributes::g_attrJsonPath).GetString()).value))
            {
                HYP_LOG(Core, Warning, "Failed to resolve JSON path \"{}\" for Class \"{}\"", pair.second->GetAttribute(Attributes::g_attrJsonPath).GetString(), instanceClass->GetName());
                continue;
            }
        }

        sortedMembers.Clear();

        // resolve data from json object to members by name
        for (const auto& [key, value] : jsonObject)
        {
            const IMember* member = instanceClass->GetMember(key.ToUtf8(), MemberType::Property | MemberType::Field);

            if (!member)
            {
                // member not found, skip
                continue;
            }

            if (member->GetMemberType() != MemberType::Property && member->GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                //  skip fields that are marked as properties
                continue;
            }

            // skip members with jsonpath attribute - they were already resolved
            if (member->GetAttribute(Attributes::g_attrJsonPath).IsValid())
            {
                continue;
            }

            // skip if jsonignore is set
            if (const ClassAttributeValue& attribute = member->GetAttribute(Attributes::g_attrJsonIgnore); attribute.IsValid() && attribute.GetBool())
            {
                continue;
            }

            if (const ClassAttributeValue& sortOrderAttribute = member->GetAttribute(Attributes::g_attrLoadOrder); sortOrderAttribute.IsValid())
            {
                sortedMembers.Insert({ sortOrderAttribute.GetInt(), member });

                continue;
            }

            // not sorted
            sortedMembers.Insert({ 0, member });
        }

        for (const KeyValuePair<int, const IMember*>& pair : sortedMembers)
        {
            if (!ResolveMember(*pair.second, *jsonObjectValue.Get(*pair.second->GetName()).value))
            {
                HYP_LOG(Core, Verbose, "Member \"{}\" for Class \"{}\" is missing or invalid, skipping", pair.second->GetName(), instanceClass->GetName());
                continue;
            }
        }

        instanceClass->PostLoad(target.ToRef().GetPointer());
    }

    if (IsGlobalContextActive<LoadAssetsFromReferencesContext>() && target.Is<AssetReference>() && targetClass != AssetReference::StaticClass())
    {
        AssetReference& assetReference = target.Get<AssetReference>();

        // target wants an AssetPath, we can give it the AssetPath
        if (targetClass == AssetPath::StaticClass())
        {
            target = BoxedValue(assetReference.GetAssetPath());

            return {};
        }

        if (assetReference.IsValid())
        {
            if (Handle<AssetObject> assetObject = assetReference.Resolve(); assetObject.IsValid())
            {
                target = BoxedValue(std::move(assetObject));

                return {};
            }
            else
            {
                // have to store path here because otherwise it will be invalidated by setting target
                const AssetPath path = assetReference.GetAssetPath();

                target = BoxedValue(Handle<ObjectBase>::Null());

                return HYP_MAKE_ERROR(Error, "Failed to load AssetObject from AssetReference: {}", path);
            }
        }

        return HYP_MAKE_ERROR(Error, "Failed to resolve AssetReference when deserializing to AssetObject: invalid reference at \"{}\"", assetReference.GetAssetPath().ToString());
    }

    return {};
}

Result BoxedFromJSON(const JSON::Value& jsonValue, const TypeInfo& typeInfo, BoxedValue& outBoxed)
{
    if (typeInfo.IsBoolType())
    {
        if (!jsonValue.IsBool())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON bool for bool but got: {}", jsonValue.ToString());
        }

        outBoxed = BoxedValue(jsonValue.AsBool());
        return {};
    }

    if (typeInfo.IsIntegralType())
    {
        if (!jsonValue.IsNumber())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON number for integral type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
        }

        const JSON::Number number = jsonValue.AsNumber();

        if (typeInfo.id == TypeId::ForType<int8>())
        {
            if (number < INT8_MIN || number > INT8_MAX)
            {
                return HYP_MAKE_ERROR(Error, "Number {} out of range for int8", number);
            }

            outBoxed = BoxedValue(int8(number));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<int16>())
        {
            if (number < INT16_MIN || number > INT16_MAX)
            {
                return HYP_MAKE_ERROR(Error, "Number {} out of range for int16", number);
            }

            outBoxed = BoxedValue(int16(number));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<int32>())
        {
            if (number < INT32_MIN || number > INT32_MAX)
            {
                return HYP_MAKE_ERROR(Error, "Number {} out of range for int32", number);
            }

            outBoxed = BoxedValue(int32(number));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<int64>())
        {
            outBoxed = BoxedValue(int64(number));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<uint8>())
        {
            if (number < 0 || number > UINT8_MAX)
            {
                return HYP_MAKE_ERROR(Error, "Number {} out of range for uint8", number);
            }

            outBoxed = BoxedValue(uint8(number));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<uint16>())
        {
            if (number < 0 || number > UINT16_MAX)
            {
                return HYP_MAKE_ERROR(Error, "Number {} out of range for uint16", number);
            }

            outBoxed = BoxedValue(uint16(number));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<uint32>())
        {
            if (number < 0 || number > UINT32_MAX)
            {
                return HYP_MAKE_ERROR(Error, "Number {} out of range for uint32", number);
            }

            outBoxed = BoxedValue(uint32(number));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<uint64>())
        {
            if (number < 0)
            {
                return HYP_MAKE_ERROR(Error, "Number {} out of range for uint64", number);
            }

            outBoxed = BoxedValue(uint64(number));
            return {};
        }

#ifdef HYP_WINDOWS
        if constexpr (!std::is_same_v<size_t, uint64> && !std::is_same_v<size_t, uint32>)
        {
            if (typeInfo.id == TypeId::ForType<size_t>())
            {
                if (number < 0 || number > SIZE_MAX)
                {
                    return HYP_MAKE_ERROR(Error, "Number out of range for size_t");
                }

                outBoxed = BoxedValue(size_t(number));
                return {};
            }
        }
#endif

        return HYP_MAKE_ERROR(Error, "Unsupported integral type");
    }

    if (typeInfo.IsFloatType())
    {
        if (!jsonValue.IsNumber())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON number for float type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
        }

        const JSON::Number number = jsonValue.AsNumber();

        if (typeInfo.id == TypeId::ForType<Float16>())
        {
            // Clamp to representable range of Float16
            const double clamped = MathUtil::Clamp(double(number), double(-FLT16_MAX), double(FLT16_MAX));
            outBoxed = BoxedValue(Float16(float(MathUtil::Clamp(double(clamped), double(-FLT16_MAX), double(FLT16_MAX)))));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<float>())
        {
            outBoxed = BoxedValue(float(MathUtil::Clamp(double(number), double(-FLT_MAX), double(FLT_MAX))));
            return {};
        }

        if (typeInfo.id == TypeId::ForType<double>())
        {
            outBoxed = BoxedValue(double(number));
            return {};
        }

        return HYP_MAKE_ERROR(Error, "Unsupported float type");
    }

    if (typeInfo.IsStringType())
    {
        if (!jsonValue.IsString())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON string for string type {}, but got: {}", typeInfo.name, jsonValue.ToString());
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;

        if (!handler || handler->GetHandlerType() != ITypeInfoHandler::TYPE_STRING)
        {
            return HYP_MAKE_ERROR(Error, "String type {} does not have a valid string handler", typeInfo.name);
        }

        ITypeInfoStringHandler* stringHandler = static_cast<ITypeInfoStringHandler*>(handler);

        BoxedValue stringInstance;
        if (!stringHandler->CreateInstance(stringInstance))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create instance of string type");
        }

        stringHandler->SetValue(stringInstance, jsonValue.AsString().ToUtf8());

        outBoxed = std::move(stringInstance);

        return {};
    }

    if (typeInfo.IsEnum() || typeInfo.IsEnumFlags())
    {
        const TypeInfo* underlyingTypeInfo = typeInfo.GetUnderlyingType();
        AssertDebug(underlyingTypeInfo != nullptr, "Enum type must have an underlying type");

        if (!underlyingTypeInfo)
        {
            return HYP_MAKE_ERROR(Error, "Enum type {} does not have a valid underlying type", typeInfo.name);
        }

        if (jsonValue.IsNumber())
        {
            if (underlyingTypeInfo->id == TypeId::ForType<int8>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt8());
                return {};
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int16>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt16());
                return {};
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int32>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt32());
                return {};
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<int64>())
            {
                outBoxed = BoxedValue(jsonValue.ToInt64());
                return {};
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint8>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt8());
                return {};
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint16>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt16());
                return {};
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint32>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt32());
                return {};
            }
            else if (underlyingTypeInfo->id == TypeId::ForType<uint64>())
            {
                outBoxed = BoxedValue(jsonValue.ToUInt64());
                return {};
            }
            else
            {
                return HYP_MAKE_ERROR(Error, "Enum type {} has unsupported underlying type {}", typeInfo.name, underlyingTypeInfo->name);
            }
        }

        /// \todo String representation of enum values

        return HYP_MAKE_ERROR(Error, "Failed to deserialize enum value for type {}", typeInfo.name);
    }

    if (typeInfo.id == TypeId::ForType<UUID>())
    {
        if (!jsonValue.IsString())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON string for UUID, but got value: {}", jsonValue.ToString());
        }

        const JSON::JString& jsonString = jsonValue.AsString();

        if (jsonString.Size() != 36)
        {
            return HYP_MAKE_ERROR(Error, "Invalid UUID string length: {}", jsonString.Size());
        }

        outBoxed = BoxedValue(UUID(*jsonString.ToAnsi()));
        return {};
    }

    if (typeInfo.id == TypeId::ForType<Name>())
    {
        if (!jsonValue.IsString())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON string for Name, but got value: {}", jsonValue.ToString());
        }

        const JSON::JString& jsonString = jsonValue.AsString();

        outBoxed = BoxedValue(Name(CreateNameFromDynamicString(*jsonString.ToAnsi())));

        return {};
    }

    if (typeInfo.IsVectorType())
    {
        if (!jsonValue.IsArray())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array for vector type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;

        if (!handler || handler->GetHandlerType() != ITypeInfoHandler::TYPE_VECTOR)
        {
            return HYP_MAKE_ERROR(Error, "Vector type {} does not have a valid vector handler", typeInfo.name);
        }

        ITypeInfoVectorHandler* vectorHandler = static_cast<ITypeInfoVectorHandler*>(handler);

        BoxedValue vectorInstance;
        if (!vectorHandler->CreateInstance(vectorInstance))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create instance of vector type");
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != vectorHandler->GetNumComponents())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array of size {} for vector type {}, but got size {}",
                vectorHandler->GetNumComponents(), typeInfo.name, jsonArray.Size());
        }

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            return HYP_MAKE_ERROR(Error, "Vector type {} does not have a valid element type", typeInfo.name);
        }

        for (int i = 0; i < int(jsonArray.Size()); i++)
        {
            BoxedValue elementData;

            if (Result result = BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData); result.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to deserialize vector element at index {} for type {}: {}", i, typeInfo.name, result.GetError().GetMessage());
            }

            vectorHandler->SetComponent(vectorInstance, i, elementData);
        }

        outBoxed = std::move(vectorInstance);

        return {};
    }

    if (typeInfo.IsMatrixType())
    {
        if (!jsonValue.IsArray())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array for matrix type {}, but got value: {}", typeInfo.name, jsonValue.ToString());
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;

        if (!handler || handler->GetHandlerType() != ITypeInfoHandler::TYPE_MATRIX)
        {
            return HYP_MAKE_ERROR(Error, "Matrix type {} does not have a valid matrix handler", typeInfo.name);
        }

        ITypeInfoMatrixHandler* matrixHandler = static_cast<ITypeInfoMatrixHandler*>(handler);

        BoxedValue matrixInstance;
        if (!matrixHandler->CreateInstance(matrixInstance))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create instance of matrix type");
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != matrixHandler->GetNumRows() * matrixHandler->GetNumColumns())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array of size {} for matrix type {}, but got size {}",
                matrixHandler->GetNumRows() * matrixHandler->GetNumColumns(), typeInfo.name, jsonArray.Size());
        }

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            return HYP_MAKE_ERROR(Error, "Matrix type {} does not have a valid element type", typeInfo.name);
        }

        for (size_t i = 0; i < jsonArray.Size(); i++)
        {
            BoxedValue elementData;

            if (Result result = BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData); result.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to deserialize matrix element at index {} for type {}", i, typeInfo.name);
            }

            matrixHandler->SetElement(matrixInstance, i / matrixHandler->GetNumColumns(), i % matrixHandler->GetNumColumns(), elementData);
        }

        outBoxed = std::move(matrixInstance);

        return {};
    }

    if (typeInfo.IsArrayType())
    {
        if (!jsonValue.IsArray())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array for array type {}, but got JSON value: {}", typeInfo.name, jsonValue.ToString());
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            return HYP_MAKE_ERROR(Error, "Array type {} does not have a valid element type", typeInfo.name);
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;
        Assert(handler && handler->GetHandlerType() == ITypeInfoHandler::TYPE_ARRAY);

        ITypeInfoArrayHandler* arrayHandler = static_cast<ITypeInfoArrayHandler*>(handler);

        BoxedValue arrayInstance;
        if (!arrayHandler->CreateInstance(arrayInstance))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create instance of array type {}", typeInfo.name);
        }

        arrayHandler->Resize(arrayInstance, jsonArray.Size());

        for (size_t i = 0; i < jsonArray.Size(); i++)
        {
            if (jsonArray[i].IsNull())
            {
                // skip null/undefined elements
                continue;
            }

            BoxedValue elementData;

            if (Result result = BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData); result.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to deserialize array element at index {} for type {}: {}", i, typeInfo.name, result.GetError().GetMessage());
            }

            if (!arrayHandler->SetElementAt(arrayInstance, i, std::move(elementData)))
            {
                return HYP_MAKE_ERROR(Error, "Failed to set array element at index {} for type {}", i, typeInfo.name);
            }
        }

        outBoxed = std::move(arrayInstance);

        return {};
    }

    if (typeInfo.IsSetType())
    {
        if (!jsonValue.IsArray())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array for set type {}, but got JSON value: {}", typeInfo.name, jsonValue.ToString());
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        const TypeInfo* elementTypeInfo = typeInfo.extendedInfo.GetElementType();

        if (!elementTypeInfo)
        {
            return HYP_MAKE_ERROR(Error, "Set type {} does not have a valid element type", typeInfo.name);
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;
        Assert(handler && handler->GetHandlerType() == ITypeInfoHandler::TYPE_SET);

        ITypeInfoSetHandler* setHandler = static_cast<ITypeInfoSetHandler*>(handler);

        BoxedValue setInstance;
        if (!setHandler->CreateInstance(setInstance))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create instance of set type {}", typeInfo.name);
        }

        for (size_t i = 0; i < jsonArray.Size(); i++)
        {
            if (jsonArray[i].IsNull())
            {
                // skip null/undefined elements
                continue;
            }

            BoxedValue elementData;

            if (Result result = BoxedFromJSON(jsonArray[i], *elementTypeInfo, elementData); result.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to deserialize set element at index {} for type: {}", i, typeInfo.name);
            }

            if (!setHandler->Insert(setInstance, elementData))
            {
                HYP_LOG(Core, Warning, "Failed to insert set element at index {} for type: {} (may be duplicate)", i, typeInfo.name);

                // Continue anyway - duplicates are expected behavior for sets
                continue;
            }
        }

        outBoxed = std::move(setInstance);

        return {};
    }

    if (typeInfo.IsMapType())
    {
        const TypeInfo* keyTypeInfo = typeInfo.extendedInfo.GetElementType();
        const TypeInfo* valueTypeInfo = typeInfo.extendedInfo.next ? typeInfo.extendedInfo.next->GetElementType() : nullptr;

        if (!keyTypeInfo)
        {
            return HYP_MAKE_ERROR(Error, "Map type {} does not have a valid key type", typeInfo.name);
        }

        if (!valueTypeInfo)
        {
            return HYP_MAKE_ERROR(Error, "Map type {} does not have a valid value type", typeInfo.name);
        }

        ITypeInfoHandler* handler = typeInfo.extendedInfo.handler;
        Assert(handler && handler->GetHandlerType() == ITypeInfoHandler::TYPE_MAP);

        ITypeInfoMapHandler* mapHandler = static_cast<ITypeInfoMapHandler*>(handler);

        BoxedValue mapInstance;
        if (!mapHandler->CreateInstance(mapInstance))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create instance of map type {}", typeInfo.name);
        }

        if (jsonValue.IsObject())
        {
            // JSON object -> string-keyed map
            const JSON::Object& jsonObj = jsonValue.AsObject();

            for (const auto& [key, val] : jsonObj)
            {
                BoxedValue keyData;
                if (Result result = BoxedFromJSON(JSON::Value(JSON::JString(key)), *keyTypeInfo, keyData); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to deserialize map key for type {}: {}", typeInfo.name, result.GetError().GetMessage());
                    continue;
                }

                BoxedValue valueData;
                if (Result result = BoxedFromJSON(val, *valueTypeInfo, valueData); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to deserialize map value for type {}: {}", typeInfo.name, result.GetError().GetMessage());
                    continue;
                }

                mapHandler->SetValueAt(mapInstance, keyData, valueData);
            }
        }
        else if (jsonValue.IsArray())
        {
            // JSON array of [key, value] pairs
            const JSON::JArray& jsonArray = jsonValue.AsArray();

            for (size_t i = 0; i < jsonArray.Size(); i++)
            {
                if (!jsonArray[i].IsArray() || jsonArray[i].AsArray().Size() != 2)
                {
                    HYP_LOG(Core, Warning, "Expected [key, value] pair at index {} for map type {}", i, typeInfo.name);
                    continue;
                }

                const JSON::JArray& pair = jsonArray[i].AsArray();

                BoxedValue keyData;
                if (Result result = BoxedFromJSON(pair[0], *keyTypeInfo, keyData); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to deserialize map key at index {} for type {}: {}", i, typeInfo.name, result.GetError().GetMessage());
                    continue;
                }

                BoxedValue valueData;
                if (Result result = BoxedFromJSON(pair[1], *valueTypeInfo, valueData); result.HasError())
                {
                    HYP_LOG(Core, Warning, "Failed to deserialize map value at index {} for type {}: {}", i, typeInfo.name, result.GetError().GetMessage());
                    continue;
                }

                mapHandler->SetValueAt(mapInstance, keyData, valueData);
            }
        }
        else
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON object or array for map type {}, but got: {}", typeInfo.name, jsonValue.ToString());
        }

        outBoxed = std::move(mapInstance);

        return {};
    }

    if (typeInfo.IsPairType())
    {
        if (!jsonValue.IsArray())
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array for pair type {}, but got JSON value: {}", typeInfo.name, jsonValue.ToString());
        }

        const JSON::JArray& jsonArray = jsonValue.AsArray();

        if (jsonArray.Size() != 2)
        {
            return HYP_MAKE_ERROR(Error, "Expected JSON array of size 2 for pair type {}, but got size {}", typeInfo.name, jsonArray.Size());
        }

        // pair typeinfo uses a linked list (extendedInfo for first, extendedInfo->next for second)

        const TypeInfo* firstTypeInfo = typeInfo.extendedInfo.GetElementType();
        Assert(firstTypeInfo != nullptr);

        const TypeInfo* secondTypeInfo = typeInfo.extendedInfo.next->GetElementType();
        Assert(secondTypeInfo != nullptr);

        BoxedValue firstData;
        if (Result result = BoxedFromJSON(jsonArray[0], *firstTypeInfo, firstData); result.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to deserialize first element of pair for type {}: {}", typeInfo.name, result.GetError().GetMessage());
        }

        BoxedValue secondData;
        if (Result result = BoxedFromJSON(jsonArray[1], *secondTypeInfo, secondData); result.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to deserialize second element of pair for type {}: {}", typeInfo.name, result.GetError().GetMessage());
        }

        ITypeInfoPairHandler* pairHandler = static_cast<ITypeInfoPairHandler*>(typeInfo.extendedInfo.handler);
        Assert(pairHandler && pairHandler->GetHandlerType() == ITypeInfoHandler::TYPE_PAIR);

        BoxedValue pairInstance;

        if (!pairHandler->CreateInstance(pairInstance))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create instance of pair type {}", typeInfo.name);
        }

        pairHandler->SetFirst(pairInstance, firstData);
        pairHandler->SetSecond(pairInstance, secondData);

        outBoxed = std::move(pairInstance);

        // ok
        return {};
    }

    if (typeInfo.IsVariantType())
    {
        ITypeInfoVariantHandler* variantHandler = static_cast<ITypeInfoVariantHandler*>(typeInfo.extendedInfo.handler);
        Assert(variantHandler && variantHandler->GetHandlerType() == ITypeInfoHandler::TYPE_VARIANT);

        if (jsonValue.IsNull())
        {
            // create empty variant on null/undefined

            BoxedValue variantInstance;
            if (!variantHandler->CreateInstance(variantInstance))
            {
                return HYP_MAKE_ERROR(Error, "Failed to create instance of variant type {}", typeInfo.name);
            }

            outBoxed = std::move(variantInstance);

            return {};
        }

        const int numTypes = variantHandler->GetNumTypes();

        // first pass: try to find type that matches (for numbers)
        // this is here because sometimes floats will get casted away to integer types,
        // even if a floating point type is actually in the variant. so we do this first pass to try filter and fit to the correct one
        for (int typeIndex = 0; typeIndex < numTypes; typeIndex++)
        {
            const TypeInfo* variantTypeInfo = variantHandler->GetTypeInfoAtIndex(typeIndex);
            AssertDebug(variantTypeInfo != nullptr, "Variant type info at index {} is null", typeIndex);

            if (!variantTypeInfo)
            {
                continue;
            }

            if (jsonValue.IsNumber() && (variantTypeInfo->IsIntegralType() || variantTypeInfo->IsFloatType()))
            {
                JSON::Number num = jsonValue.AsNumber();

                const bool isInteger = MathUtil::Fract(num) == 0;

                if (isInteger == variantTypeInfo->IsIntegralType())
                {
                    BoxedValue variantInstance;

                    if (!variantHandler->CreateInstance(variantInstance))
                    {
                        return HYP_MAKE_ERROR(Error, "Failed to create instance of variant type {}", typeInfo.name);
                    }

                    bool bSuccess = false;

                    if (variantTypeInfo->IsIntegralType())
                    {
                        bSuccess = variantHandler->SetValue(variantInstance, BoxedValue(static_cast<int64>(num)));
                    }
                    else
                    {
                        bSuccess = variantHandler->SetValue(variantInstance, BoxedValue(static_cast<double>(num)));
                    }

                    if (bSuccess)
                    {
                        outBoxed = std::move(variantInstance);
                        return {};
                    }
                }
            }
        }

        // second pass: use first one that works
        for (int typeIndex = 0; typeIndex < numTypes; typeIndex++)
        {
            const TypeInfo* variantTypeInfo = variantHandler->GetTypeInfoAtIndex(typeIndex);
            AssertDebug(variantTypeInfo != nullptr, "Variant type info at index {} is null", typeIndex);

            if (!variantTypeInfo)
            {
                continue;
            }

            BoxedValue variantData;

            if (Result result = BoxedFromJSON(jsonValue, *variantTypeInfo, variantData); !result.HasError())
            {
                BoxedValue variantInstance;

                if (!variantHandler->CreateInstance(variantInstance))
                {
                    return HYP_MAKE_ERROR(Error, "Failed to create instance of variant type {}", typeInfo.name);
                }

                if (!variantHandler->SetValue(variantInstance, variantData))
                {
                    return HYP_MAKE_ERROR(Error, "Failed to set value of variant type {}", typeInfo.name);
                }

                outBoxed = std::move(variantInstance);
                return {};
            }
        }

        return HYP_MAKE_ERROR(Error, "Failed to deserialize JSON to any of the possible types for variant: {}", typeInfo.name);
    }

    if (jsonValue.IsNull() && typeInfo.IsClass())
    {
        // null object
        outBoxed = BoxedValue();

        return {};
    }

    // Object types
    if (jsonValue.IsObject())
    {
        const Class* typeInfoClass = typeInfo.GetClass();
        const Class* instanceClass = typeInfoClass;

        // first check for $Class property to override type
        if (auto classValue = jsonValue.Get("$Class"); classValue && classValue.IsString())
        {
            const Class* derivedClass = GetClass(StringHash(classValue.AsString()));

            if (derivedClass)
            {
                if (instanceClass != nullptr
                    && !(derivedClass->IsDerivedFrom(instanceClass)
                        // We allow $Class to be 'AssetReference', but only for AssetObject classes. (if LoadAssetsFromReferencesContext is active)
                        || (IsGlobalContextActive<LoadAssetsFromReferencesContext>() && derivedClass == AssetReference::StaticClass() && instanceClass->IsDerivedFrom(AssetObject::StaticClass()))))
                {
                    HYP_LOG(Core, Warning, "Class '{}' is not derived from expected class '{}'", derivedClass->GetName(), instanceClass->GetName());
                }
            }
            else
            {
                HYP_LOG(Core, Warning, "Class '{}' not found!", classValue.AsString());
            }

            instanceClass = derivedClass;
        }

        if (instanceClass)
        {
            BoxedValue instance;

            if (!instanceClass->CreateInstance(instance))
            {
                return HYP_MAKE_ERROR(Error, "Failed to create instance of Class \"{}\"", instanceClass->GetName());
            }

            if (Result result = ObjectFromJSON(jsonValue.AsObject(), typeInfoClass, instance); result.HasError())
            {
                return HYP_MAKE_ERROR(Error, "Failed to deserialize instance of \"{}\" from JSON", instanceClass->GetName());
            }

            outBoxed = std::move(instance);

            return {};
        }

        return HYP_MAKE_ERROR(Error, "Could not find Class for type: {}", typeInfo.name);
    }

    return HYP_MAKE_ERROR(Error, "Failed to deserialize JSON to BoxedValue of type: {}, no handle logic for JSON value: {}",
        typeInfo.name, jsonValue.ToString(true));
}

void WalkBoxedValue(
    const BoxedValue& target,
    const ProcRef<void(const BoxedValue& current)>& func)
{
    Assert(target.IsValid());

    if (!target.IsValid())
    {
        return;
    }

    const TypeInfo& typeInfo = *target.GetTypeInfo();

    if (typeInfo.IsArrayType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_ARRAY);

        ITypeInfoArrayHandler* handler = static_cast<ITypeInfoArrayHandler*>(typeInfo.extendedInfo.handler);

        const size_t size = handler->GetSize(target);

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        for (size_t i = 0; i < size; i++)
        {
            BoxedValue element;

            if (!handler->GetElementAt(target, i, element))
            {
                HYP_LOG(Core, Warning, "Failed to get element at index {} of array of type {}", i, typeInfo.name);

                continue;
            }

            func(element);
        }

        return;
    }

    if (typeInfo.IsSetType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_SET);

        ITypeInfoSetHandler* handler = static_cast<ITypeInfoSetHandler*>(typeInfo.extendedInfo.handler);

        const size_t size = handler->GetSize(target);

        JSON::JArray jsonArray;
        jsonArray.Reserve(size);

        // Use iterator to traverse set elements
        ITypeInfoIterator* iterator = handler->CreateIterator(target);
        HYP_DEFER({ delete iterator; });

        Assert(iterator != nullptr);

        while (iterator->HasNext())
        {
            AnyRef element = iterator->GetCurrent();

            if (!element.HasValue())
            {
                HYP_LOG(Core, Warning, "Failed to get current element from set iterator of type {}", typeInfo.name);
                iterator->Next();
                continue;
            }

            func(BoxedValue(element));

            iterator->Next();
        }

        return;
    }

    if (typeInfo.IsMapType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_MAP);

        ITypeInfoMapHandler* handler = static_cast<ITypeInfoMapHandler*>(typeInfo.extendedInfo.handler);

        ITypeInfoIterator* iterator = handler->CreateIterator(target);
        HYP_DEFER({ delete iterator; });

        Assert(iterator != nullptr);

        while (iterator->HasNext())
        {
            AnyRef keyRef = iterator->GetKey();
            AnyRef valueRef = iterator->GetCurrent();

            if (keyRef.HasValue())
            {
                func(BoxedValue(keyRef));
            }

            if (valueRef.HasValue())
            {
                func(BoxedValue(valueRef));
            }

            iterator->Next();
        }

        return;
    }

    if (typeInfo.IsPairType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_PAIR);

        ITypeInfoPairHandler* pairHandler = static_cast<ITypeInfoPairHandler*>(typeInfo.extendedInfo.handler);

        BoxedValue firstBoxed;
        pairHandler->GetFirst(target, firstBoxed);

        BoxedValue secondBoxed;
        pairHandler->GetSecond(target, secondBoxed);

        func(firstBoxed);
        func(secondBoxed);

        return;
    }

    if (typeInfo.IsVariantType())
    {
        Assert(typeInfo.extendedInfo.handler && typeInfo.extendedInfo.handler->GetHandlerType() == ITypeInfoHandler::TYPE_VARIANT);

        ITypeInfoVariantHandler* handler = static_cast<ITypeInfoVariantHandler*>(typeInfo.extendedInfo.handler);

        if (handler->GetCurrentTypeIndex(target) == Variant<std::nullptr_t>::invalidTypeIndex)
        {
            return;
        }

        AnyRef activeValue = handler->GetValue(target);

        if (!activeValue.HasValue())
        {
            // no active value.
            return;
        }

        return func(BoxedValue(activeValue));
    }
}

void StripTransientMembers(BoxedValue& value)
{
    if (!value.IsValid())
    {
        return;
    }

    const TypeInfo& typeInfo = *value.GetTypeInfo();
    const Class* cls = typeInfo.GetClass();

    if (!cls)
    {
        return;
    }

    // Build a set of transient member names so we know which to skip
    FlatSet<Name> transientMembers;

    for (const IMember& member : cls->GetMembers(MemberType::Field | MemberType::Property, /* deep */ false))
    {
        const ClassAttributeValue& transientAttr = member.GetAttribute(Attributes::g_attrTransient);

        if (transientAttr.IsValid() && transientAttr.GetBool())
        {
            transientMembers.Insert(member.GetName());
        }
    }

    if (transientMembers.Empty())
    {
        return;
    }

    // Create a default instance of the same class
    BoxedValue defaultValue;

    if (!cls->CreateInstance(defaultValue))
    {
        return;
    }

    // Copy default values for transient members from the default instance to the target
    for (const IMember& member : cls->GetMembers(MemberType::Field | MemberType::Property, /* deep */ false))
    {
        if (!transientMembers.Contains(member.GetName()))
        {
            continue;
        }

        BoxedValue memberDefaultValue;

        switch (member.GetMemberType())
        {
        case MemberType::Field:
            memberDefaultValue = static_cast<const Field&>(member).Get(defaultValue);
            static_cast<const Field&>(member).Set(value, memberDefaultValue);
            break;
        case MemberType::Property:
            memberDefaultValue = static_cast<const Property&>(member).Get(defaultValue);
            static_cast<const Property&>(member).Set(value, memberDefaultValue);
            break;
        default:
            break;
        }
    }
}

bool CloneWithoutTransientMembers(const BoxedValue& src, BoxedValue& outDst)
{
    if (!src.IsValid())
    {
        return false;
    }

    const TypeInfo& typeInfo = *src.GetTypeInfo();
    const Class* cls = typeInfo.GetClass();

    if (!cls)
    {
        outDst = src;
        return true;
    }

    if (!cls->CreateInstance(outDst))
    {
        return false;
    }

    for (const IMember& member : cls->GetMembers(MemberType::Field | MemberType::Property, /* deep */ false))
    {
        const ClassAttributeValue& transientAttr = member.GetAttribute(Attributes::g_attrTransient);
        if (transientAttr.IsValid() && transientAttr.GetBool())
        {
            continue;
        }

        BoxedValue srcValue;

        switch (member.GetMemberType())
        {
        case MemberType::Field:
            srcValue = static_cast<const Field&>(member).Get(src);
            static_cast<const Field&>(member).Set(outDst, srcValue);
            break;
        case MemberType::Property:
            srcValue = static_cast<const Property&>(member).Get(src);
            static_cast<const Property&>(member).Set(outDst, srcValue);
            break;
        default:
            break;
        }
    }

    return true;
}

// Dependency injection on static initialization to set these function pointers
static struct ConfigBaseDependencyInject
{
    ConfigBaseDependencyInject()
    {
        ConfigBase::s_ObjectToJSON = &ObjectToJSON;
        ConfigBase::s_ObjectFromJSON = &ObjectFromJSON;
    }
} s_configBaseDependencyInject {};

} // namespace Hyperion
