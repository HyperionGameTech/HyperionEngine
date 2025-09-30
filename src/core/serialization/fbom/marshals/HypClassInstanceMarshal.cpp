/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypObjectBase.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <core/utilities/GlobalContext.hpp>
#endif

namespace hyperion::serialization {

static const TypeId g_typeIdAssetReference = TypeId::ForType<AssetReference>();
static const Name g_nameResolveAsset = NAME("resolveasset");

static void CollectAssetReferenceMembers(
    const HypClass* hypClass,
    Array<Pair<const IHypMember*, const IHypMember*>>& outMembers)
{
    HashSet<Name> usedMemberNames;

    for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD))
    {
        if (member.GetMemberType() != HypMemberType::TYPE_PROPERTY && member.GetAttribute("property").IsValid())
        {
            // skip fields with synthetic properties
            continue;
        }

        if (member.GetTypeId() == g_typeIdAssetReference)
        {
            if (usedMemberNames.Contains(member.GetName()))
            {
                HYP_LOG(Serialization, Warning, "Member '{}' of HypClass '{}' is shadows another member with the same name, skipping serialization", member.GetName(), member.GetOwnerClass()->GetName());

                continue;
            }

            const IHypMember* pTargetMember = nullptr;

            if (const HypClassAttributeValue& targetAttr = member.GetAttribute(g_nameResolveAsset))
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
    else if (hypClass->IsDerivedFrom(AssetObject::Class()) && IsGlobalContextActive<EditorProjectSaveContext>())
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

#ifdef HYP_DEBUG_MODE
    // Check that the asset being referred to is not in a transient package -- deserialization will result in an asset that can't be found.
    if (assetReference.IsValid())
    {
        const Handle<AssetObject>& assetObject = assetReference.Resolve();

        if (!assetObject || !assetObject->GetPackage() || assetObject->GetPackage()->IsTransient())
        {
            HYP_BREAKPOINT;

            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize AssetReference to asset '{}' because it is not loaded or in a transient package", assetReference.GetAssetPath().ToString()) };
        }
    }
#endif

    // If we have an asset reference to serialize, we serialize that instead of the actual object inline.
    // This will reduce duplication and file sizes.
    if (isAssetObject && assetReference.IsValid())
    {
        return Serialize(ConstAnyRef(assetReference), out);
    }

    assetReference = AssetReference();
#endif

    const HypClassAttributeValue& serializeAttribute = hypClass->GetAttribute("serialize");

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

    HypData targetData { AnyRef { hypClass->GetTypeId(), const_cast<void*>(in.GetPointer()) } };

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

        for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD))
        {
            if (member.IsDelegate())
            {
                continue;
            }

            if (member.GetMemberType() != HypMemberType::TYPE_PROPERTY && member.GetAttribute("property").IsValid())
            {
                // skip fields with synthetic properties (we'll serialize the property instead)
                continue;
            }

            if (!member.GetAttribute("serialize").GetBool(true) || member.GetAttribute("transient").GetBool(false))
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

            if (member.GetAttribute("compressed").GetBool())
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

    AnyRef ref = out.ToRef();
    HYP_CORE_ASSERT(ref.HasValue(), "Failed to create HypClass instance");

    return Deserialize_Internal(context, in, hypClass, ref);
}

FBOMResult HypClassInstanceMarshal::Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const HypClass* hypClass, AnyRef ref) const
{
    HYP_CORE_ASSERT(hypClass != nullptr);
    HYP_CORE_ASSERT(ref.HasValue());

    // @TODO: Handle AssetObject derived types the same way we do in serialization

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

                if (!pMember->GetAttribute("serialize").GetBool(true) || pMember->GetAttribute("transient").GetBool(false))
                {
                    continue;
                }

                if (!pMember->CanDeserialize())
                {
                    HYP_NAMED_SCOPE_FMT("Deserializing member '{}' on object, skipping setter for HypClass '{}'", pMember->GetName(), hypClass->GetName());

                    HYP_LOG(Serialization, Warning, "Member '{}' of HypClass '{}' is not deserializable, skipping", pMember->GetName(), hypClass->GetName());

                    continue;
                }

                const auto assetReferenceMembersIt = assetReferenceMembersMap.Find(pMember);

                if (assetReferenceMembersIt != assetReferenceMembersMap.End())
                {
                    const IHypMember* pTargetMember = assetReferenceMembersIt->second;

                    if (pTargetMember)
                    {
                        AssertDebug(pMember->GetTypeId() == g_typeIdAssetReference);

                        AssetReference* pAssetReference = nullptr;
                        HypData tmpData;

                        switch (pMember->GetMemberType())
                        {
                        case HypMemberType::TYPE_FIELD:
                            tmpData = static_cast<const HypField*>(pMember)->Get(HypData(AnyRef(ref)));
                            break;
                        case HypMemberType::TYPE_PROPERTY:
                            tmpData = static_cast<const HypProperty*>(pMember)->Get(HypData(AnyRef(ref)));
                            break;
                        default:
                            HYP_UNREACHABLE();
                            break;
                        }

                        pAssetReference = reinterpret_cast<AssetReference*>(tmpData.ToRef().GetPointer());

                        if (!pAssetReference)
                        {
                            HYP_LOG(Serialization, Warning, "Failed to get AssetReference for member '{}' on object of HypClass '{}'", pMember->GetName(), hypClass->GetName());

                            continue;
                        }

                        Handle<AssetObject> assetObject = ResolveAssetImpl(*pAssetReference);

                        if (!assetObject)
                        {
                            HYP_LOG(Serialization, Warning, "Failed to resolve AssetReference for member '{}' on object of HypClass '{}'", pMember->GetName(), hypClass->GetName());

                            continue;
                        }

                        HypData targetData { AnyRef(ref) };

                        switch (pTargetMember->GetMemberType())
                        {
                        case HypMemberType::TYPE_FIELD:
                            static_cast<const HypField*>(pTargetMember)->Set(targetData, HypData(std::move(assetObject)));
                            break;
                        case HypMemberType::TYPE_PROPERTY:
                            static_cast<const HypProperty*>(pTargetMember)->Set(targetData, HypData(std::move(assetObject)));
                            break;
                        default:
                            HYP_UNREACHABLE();
                            break;
                        }

                        continue;
                    }
                }

                HypData targetData { AnyRef(ref) };

                if (Result deserializeResult = pMember->Deserialize(context, targetData, it.second); deserializeResult.HasError())
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

    hypClass->PostLoad(ref.GetPointer());

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::serialization
