/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/reflection/HypClass.hpp>
#include <core/reflection/HypStruct.hpp>
#include <core/reflection/HypProperty.hpp>
#include <core/reflection/HypField.hpp>
#include <core/reflection/HypMethod.hpp>
#include <core/reflection/HypObjectBase.hpp>

#include <core/utilities/Format.hpp>
#include <core/reflection/TypeInfo.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <core/utilities/GlobalContext.hpp>
#endif

namespace hyperion::serialization {

static const TypeId s_typeIdAssetReference = TypeId::ForType<AssetReference>();
static const Name s_nameResolveAsset = NAME("resolveasset");

struct SaveAssetsAsReferencesContext
{
};

struct LoadAssetsFromReferencesContext
{
};

static void CollectAssetReferenceMembers(
    const HypClass* hypClass,
    Array<Pair<const IHypMember*, const IHypMember*>>& outMembers)
{
    HashSet<Name> usedMemberNames;

    for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD))
    {
        if (member.GetMemberType() != HypMemberType::TYPE_PROPERTY && member.GetAttribute(Attributes::g_attrProperty).IsValid())
        {
            // skip fields with synthetic properties
            continue;
        }

        if (member.GetTypeId() == s_typeIdAssetReference)
        {
            if (usedMemberNames.Contains(member.GetName()))
            {
                HYP_LOG(Serialization, Warning, "Member '{}' of HypClass '{}' is shadows another member with the same name, skipping serialization", member.GetName(), member.GetOwnerClass()->GetName());

                continue;
            }

            const IHypMember* pTargetMember = nullptr;

            if (const HypClassAttributeValue& targetAttr = member.GetAttribute(s_nameResolveAsset))
            {
                // Get the target member
                pTargetMember = hypClass->GetMember(*targetAttr.GetString(), HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD);

                if (!pTargetMember)
                {
                    HYP_LOG(Serialization, Warning, "Member '{}' of HypClass '{}' has 'ResolveAsset' attribute set to '{}', but no such member was found in the HypClass, skipping serialization", member.GetName(), member.GetOwnerClass()->GetName(), *targetAttr.GetString());

                    continue;
                }

                if (pTargetMember->IsDelegate())
                {
                    HYP_LOG(Serialization, Warning, "Member '{}' of HypClass '{}' has 'ResolveAsset' attribute set to '{}', but that member is a delegate, skipping serialization", member.GetName(), member.GetOwnerClass()->GetName(), *targetAttr.GetString());

                    continue;
                }
            }

            outMembers.EmplaceBack(&member, pTargetMember);
            usedMemberNames.Insert(member.GetName());
        }
    }
}

FBOMResult HypClassInstanceMarshal::Serialize(ConstAnyRef in, FBOMObject& out) const
{
    if (!in.HasValue())
    {
        return { FBOMResult::FBOM_ERR, "Attempting to serialize null object" };
    }

    const HypClass* hypClass = GetClass(in.GetTypeId());

    if (!hypClass)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeId {} has no associated HypClass", LookupTypeName(in.GetTypeId())) };
    }

    if (hypClass->IsClassType())
    {
        // Get instance class if object is a HypObject
        const HypClass* instanceClass = static_cast<const HypObjectBase*>(in.GetPointer())->InstanceClass();
        Assert(instanceClass != nullptr);

        hypClass = instanceClass;
    }

    if (!hypClass->CanSerialize())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeId {} has no associated HypClass", LookupTypeName(in.GetTypeId())) };
    }

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE

    // asset reference to serialize in place of serializing an actual object (used for AssetObject deriving classes)
    AssetReference assetReference;
    bool isAssetObject = false;

    if (hypClass->IsDerivedFrom(AssetReference::Class()))
    {
        assetReference = in.Get<AssetReference>();
    }
    else if (hypClass->IsDerivedFrom(AssetObject::Class()))
    {
        // Serialize AssetObject deriving classes by their asset reference if we're saving an Editor project.
        const AssetObject& assetObject = in.Get<AssetObject>();
        AssertDebug(assetObject.IsRegistered(), "Cannot serialize unregistered AssetObject");

        if (!assetObject.IsRegistered())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize AssetObject '{}' because it is not registered to a package!", assetObject.GetPath().ToString()) };
        }

        assetReference = AssetReference(assetObject.HandleFromThis());
        isAssetObject = true;
    }

#if 0 // def HYP_DEBUG_MODE
    // Check that the asset being referred to is not in a transient package -- deserialization will result in an asset that can't be found.
    if (assetReference.IsValid())
    {
        const Handle<AssetObject>& assetObject = assetReference.Resolve();

        if (!assetObject || !assetObject->GetPackage() || assetObject->GetPackage()->IsTransient())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize AssetReference to asset '{}' because it is not loaded or in a transient package", assetReference.GetAssetPath().ToString()) };
        }
    }
#endif

    // If we have an asset reference to serialize, we serialize that instead of the actual object inline.
    // This will reduce duplication and file sizes.
    if (isAssetObject && assetReference.IsValid() && IsGlobalContextActive<SaveAssetsAsReferencesContext>())
    {
        return Serialize(ConstAnyRef(assetReference), out);
    }

    assetReference = AssetReference();
#endif

    const HypClassAttributeValue& serializeAttribute = hypClass->GetAttribute(Attributes::g_attrSerialize);

    if (!serializeAttribute.GetBool(true))
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=false", hypClass->GetName()) };
    }

    HYP_NAMED_SCOPE_FMT("Serializing object with HypClass '{}'", hypClass->GetName());

    if (hypClass->GetSerializationMode() & HypClassSerializationMode::BITWISE)
    {
        if (!hypClass->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hypClass->GetName()) };
        }

        const HypStruct* hypStruct = static_cast<const HypStruct*>(hypClass);

        if (FBOMResult err = hypStruct->SerializeStruct(in, out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}': {}", hypClass->GetName(), err.message) };
        }

        return { FBOMResult::FBOM_OK };
    }

    const TypeInfo& typeInfo = TypeInfo::ForHypClass(hypClass);

    HypData targetData { AnyRef(&typeInfo, const_cast<void*>(in.GetPointer())) };

    out = FBOMObject(FBOMObjectType(hypClass));

    HashSet<Name> serializedMembers;

    HashSet<const IHypMember*> assetReferenceTargetMembersMap;
    HashMap<const IHypMember*, const IHypMember*> assetReferenceMembersMap;

    { // collect asset reference members
        Array<Pair<const IHypMember*, const IHypMember*>> assetReferenceMembers;
        CollectAssetReferenceMembers(hypClass, assetReferenceMembers);

        for (const auto& pair : assetReferenceMembers)
        {
            assetReferenceMembersMap.Set(pair.first, pair.second);

            if (pair.second != nullptr)
            {
                assetReferenceTargetMembersMap.Insert(pair.second);
            }
        }
    }

    {
        HYP_NAMED_SCOPE_FMT("Serializing properties for HypClass '{}'", hypClass->GetName());

        // Save AssetObject as AssetReferences for nested fields / objects.
        GlobalContextScope contextScope { SaveAssetsAsReferencesContext {} };

        for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD))
        {
            if (member.IsDelegate())
            {
                continue;
            }

            if (member.GetMemberType() != HypMemberType::TYPE_PROPERTY && member.GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                // skip fields with synthetic properties (we'll serialize the property instead)
                continue;
            }

            if (!member.GetAttribute(Attributes::g_attrSerialize).GetBool(true) || member.GetAttribute(Attributes::g_attrTransient).GetBool(false))
            {
                continue;
            }

            if (!member.CanSerialize())
            {
                continue;
            }

            EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE;

            if (assetReferenceTargetMembersMap.Contains(&member))
            {
                continue;
            }

            if (!serializedMembers.Insert(member.GetName()).second)
            {
                HYP_LOG(Serialization, Warning, "Member '{}' of HypClass '{}' shadows another member with the same name, skipping serialization", member.GetName(), hypClass->GetName());
                continue;
            }

            if (member.GetAttribute(Attributes::g_attrCompressed).GetBool())
            {
                flags |= FBOMDataFlags::COMPRESSED;
            }

            HYP_NAMED_SCOPE_FMT("Serializing member '{}' for HypClass '{}'", member.GetName(), hypClass->GetName());

            FBOMData data;

            if (Result serializeResult = member.Serialize(Span<HypData>(&targetData, 1), data, flags); serializeResult.HasError())
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to serialize member '{}' of HypClass '{}': {}", member.GetName(), hypClass->GetName(), serializeResult.GetError().GetMessage()) };
            }

            out.SetProperty(member.GetName().LookupString(), std::move(data));
        }
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const
{
    const HypClass* hypClass = in.GetHypClass();

    if (!hypClass)
    {
        HYP_BREAKPOINT;
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, serialized data with type '{}' (TypeId: {}) has no associated HypClass", in.GetType().name, in.GetType().GetNativeTypeId().Value()) };
    }

    if (!hypClass->CreateInstance(out))
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, HypClass '{}' instance creation failed", hypClass->GetName()) };
    }

    if (hypClass->GetSerializationMode() & HypClassSerializationMode::BITWISE)
    {
        if (!hypClass->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hypClass->GetName()) };
        }

        const HypStruct* hypStruct = static_cast<const HypStruct*>(hypClass);

        return hypStruct->DeserializeStruct(context, in, out);
    }

    return Deserialize_Internal(context, in, hypClass, out);
}

FBOMResult HypClassInstanceMarshal::Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const HypClass* hypClass, HypData& target) const
{
    Assert(hypClass != nullptr);
    Assert(target.IsValid());

    HashSet<const IHypMember*> assetReferenceTargetMembersMap;
    HashMap<const IHypMember*, const IHypMember*> assetReferenceMembersMap;

    { // collect asset reference members
        Array<Pair<const IHypMember*, const IHypMember*>> assetReferenceMembers;
        CollectAssetReferenceMembers(hypClass, assetReferenceMembers);

        for (const auto& pair : assetReferenceMembers)
        {
            assetReferenceMembersMap.Set(pair.first, pair.second);

            if (pair.second != nullptr)
            {
                assetReferenceTargetMembersMap.Insert(pair.second);
            }
        }
    }

    {
        HYP_NAMED_SCOPE_FMT("Deserializing properties for HypClass '{}'", hypClass->GetName());

        GlobalContextScope contextScope { LoadAssetsFromReferencesContext {} };

        for (const KeyValuePair<ANSIString, FBOMData>& it : in.GetProperties())
        {
            if (const IHypMember* pMember = hypClass->GetMember(it.first, HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD))
            {
                if (pMember->IsDelegate())
                {
                    continue;
                }

                if (assetReferenceTargetMembersMap.Contains(pMember))
                {
                    continue;
                }

                if (!pMember->GetAttribute(Attributes::g_attrDeserialize).GetBool(true) || pMember->GetAttribute(Attributes::g_attrTransient).GetBool(false))
                {
                    continue;
                }

                if (!pMember->CanDeserialize())
                {
                    HYP_NAMED_SCOPE_FMT("Deserializing member '{}' on object, skipping setter for HypClass '{}'", pMember->GetName(), hypClass->GetName());

                    HYP_LOG(Serialization, Warning, "Member '{}' of HypClass '{}' is not deserializable, skipping", pMember->GetName(), hypClass->GetName());

                    continue;
                }

                if (Result deserializeResult = pMember->Deserialize(context, target, it.second); deserializeResult.HasError())
                {
                    return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to deserialize member '{}' of HypClass '{}': {}", pMember->GetName(), hypClass->GetName(), deserializeResult.GetError().GetMessage()) };
                }
            }
            else
            {
                HYP_LOG(Serialization, Warning, "No member named '{}' found in HypClass '{}', skipping", it.first, hypClass->GetName());
            }
        }
    }

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    if (IsGlobalContextActive<LoadAssetsFromReferencesContext>() && target.GetTypeInfo()->GetHypClass() == AssetReference::Class())
    {
        // resolve asset reference to asset object
        AssetReference& assetReference = target.Get<AssetReference>();
        const Handle<AssetObject>& assetObject = ResolveAssetImpl(assetReference);

        if (assetObject)
        {
            target = HypData(assetObject);

            return {};
        }

        HYP_LOG(Serialization, Warning, "Failed to resolve AssetReference for asset '{}'", assetReference.GetAssetPath().ToString());

        return {};
    }
#endif

    hypClass->PostLoad(target.ToRef().GetPointer());

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::serialization
