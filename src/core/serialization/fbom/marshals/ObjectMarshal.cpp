/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/marshals/ObjectMarshal.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/reflection/Class.hpp>
#include <core/reflection/Struct.hpp>
#include <core/reflection/Property.hpp>
#include <core/reflection/Field.hpp>
#include <core/reflection/Method.hpp>
#include <core/reflection/Object.hpp>
#include <core/reflection/TypeInfo.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>

#include <core/utilities/GlobalContext.hpp>
#endif

namespace Hyperion::serialization {

static const TypeId s_typeIdAssetReference = TypeId::ForType<AssetReference>();
static const Name s_nameResolveAsset = NAME("resolveasset");

struct SaveAssetsAsReferencesContext
{
};

struct LoadAssetsFromReferencesContext
{
};

static void CollectAssetReferenceMembers(
    const Class* cls,
    Array<Pair<const IMember*, const IMember*>>& outMembers)
{
    HashSet<Name> usedMemberNames;

    for (IMember& member : cls->GetMembers(MemberType::Property | MemberType::Field))
    {
        if (member.GetMemberType() != MemberType::Property && member.GetAttribute(Attributes::g_attrProperty).IsValid())
        {
            // skip fields with synthetic properties
            continue;
        }

        if (member.GetTypeId() == s_typeIdAssetReference)
        {
            if (usedMemberNames.Contains(member.GetName()))
            {
                HYP_LOG(Serialization, Warning, "Member '{}' of Class '{}' is shadows another member with the same name, skipping serialization", member.GetName(), member.GetOwnerClass()->GetName());

                continue;
            }

            const IMember* pTargetMember = nullptr;

            if (const ClassAttributeValue& targetAttr = member.GetAttribute(s_nameResolveAsset))
            {
                // Get the target member
                pTargetMember = cls->GetMember(*targetAttr.GetString(), MemberType::Property | MemberType::Field);

                if (!pTargetMember)
                {
                    HYP_LOG(Serialization, Warning, "Member '{}' of Class '{}' has 'ResolveAsset' attribute set to '{}', but no such member was found in the Class, skipping serialization", member.GetName(), member.GetOwnerClass()->GetName(), *targetAttr.GetString());

                    continue;
                }

                if (pTargetMember->IsDelegate())
                {
                    HYP_LOG(Serialization, Warning, "Member '{}' of Class '{}' has 'ResolveAsset' attribute set to '{}', but that member is a delegate, skipping serialization", member.GetName(), member.GetOwnerClass()->GetName(), *targetAttr.GetString());

                    continue;
                }
            }

            outMembers.EmplaceBack(&member, pTargetMember);
            usedMemberNames.Insert(member.GetName());
        }
    }
}

FBOMType ObjectMarshal::GetObjectType() const
{
    return FBOMObjectType("ClassStub");
}

TypeId ObjectMarshal::GetTypeId() const
{
    return TypeId::ForType<ClassStub>();
}

FBOMResult ObjectMarshal::Serialize(ConstAnyRef in, FBOMObject& out) const
{
    if (!in.HasValue())
    {
        return { FBOMResult::FBOM_ERR, "Attempting to serialize null object" };
    }

    const Class* cls = GetClass(in.GetTypeId());

    if (!cls)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using ObjectMarshal, TypeId {} has no associated Class", LookupTypeName(in.GetTypeId())) };
    }

    if (cls->IsClassType())
    {
        // Get instance class if object is a Object
        const Class* instanceClass = static_cast<const ObjectBase*>(in.GetPointer())->InstanceClass();
        Assert(instanceClass != nullptr);

        cls = instanceClass;
    }

    if (!cls->CanSerialize())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using ObjectMarshal, TypeId {} has no associated Class", LookupTypeName(in.GetTypeId())) };
    }

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE

    // asset reference to serialize in place of serializing an actual object (used for AssetObject deriving classes)
    AssetReference assetReference;
    bool isAssetObject = false;

    if (cls->IsDerivedFrom(AssetReference::StaticClass()))
    {
        assetReference = in.Get<AssetReference>();
    }
    else if (cls->IsDerivedFrom(AssetObject::StaticClass()))
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

    const ClassAttributeValue& serializeAttribute = cls->GetAttribute(Attributes::g_attrSerialize);

    if (!serializeAttribute.GetBool(true))
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with Class '{}', Class has attribute \"serialize\"=false", cls->GetName()) };
    }

    HYP_NAMED_SCOPE_FMT("Serializing object with Class '{}'", cls->GetName());

    if (cls->GetSerializationMode() & ClassSerializationMode::BITWISE)
    {
        if (!cls->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with Class '{}', Class has attribute \"serialize\"=\"bitwise\" but is not a struct type", cls->GetName()) };
        }

        const Struct* pStruct = static_cast<const Struct*>(cls);

        if (FBOMResult err = pStruct->SerializeStruct(in, out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with Class '{}': {}", cls->GetName(), err.message) };
        }

        return { FBOMResult::FBOM_OK };
    }

    const TypeInfo* pTypeInfo = cls->GetTypeInfo();
    AssertDebug(pTypeInfo != nullptr);

    BoxedValue targetData { AnyRef(pTypeInfo, const_cast<void*>(in.GetPointer())) };

    out = FBOMObject(FBOMObjectType(cls));

    HashSet<Name> serializedMembers;

    HashSet<const IMember*> assetReferenceTargetMembersMap;
    HashMap<const IMember*, const IMember*> assetReferenceMembersMap;

    { // collect asset reference members
        Array<Pair<const IMember*, const IMember*>> assetReferenceMembers;
        CollectAssetReferenceMembers(cls, assetReferenceMembers);

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
        HYP_NAMED_SCOPE_FMT("Serializing properties for Class '{}'", cls->GetName());

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
        // Save AssetObject as AssetReferences for nested fields / objects.
        GlobalContextScope contextScope { SaveAssetsAsReferencesContext {} };
#endif

        for (IMember& member : cls->GetMembers(MemberType::Property | MemberType::Field))
        {
            if (member.IsDelegate())
            {
                continue;
            }

            if (member.GetMemberType() != MemberType::Property && member.GetAttribute(Attributes::g_attrProperty).IsValid())
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
                HYP_LOG(Serialization, Warning, "Member '{}' of Class '{}' shadows another member with the same name, skipping serialization", member.GetName(), cls->GetName());
                continue;
            }

            if (member.GetAttribute(Attributes::g_attrCompressed).GetBool())
            {
                flags |= FBOMDataFlags::COMPRESSED;
            }

            HYP_NAMED_SCOPE_FMT("Serializing member '{}' for Class '{}'", member.GetName(), cls->GetName());

            FBOMData data;

            if (Result serializeResult = member.Serialize(Span<BoxedValue>(&targetData, 1), data, flags); serializeResult.HasError())
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to serialize member '{}' of Class '{}': {}", member.GetName(), cls->GetName(), serializeResult.GetError().GetMessage()) };
            }

            out.SetProperty(member.GetName().LookupString(), std::move(data));
        }
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult ObjectMarshal::Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const
{
    const Class* cls = in.GetClass();

    if (!cls)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using ObjectMarshal, serialized data with type '{}' (TypeId: {}) has no associated Class", in.GetType().name, in.GetType().GetNativeTypeId().Value()) };
    }

    if (!cls->CreateInstance(out))
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using ObjectMarshal, Class '{}' instance creation failed", cls->GetName()) };
    }

    if (cls->GetSerializationMode() & ClassSerializationMode::BITWISE)
    {
        if (!cls->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with Class '{}', Class has attribute \"serialize\"=\"bitwise\" but is not a struct type", cls->GetName()) };
        }

        const Struct* pStruct = static_cast<const Struct*>(cls);

        return pStruct->DeserializeStruct(context, in, out);
    }

    return Deserialize_Internal(context, in, cls, out);
}

FBOMResult ObjectMarshal::Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const Class* cls, BoxedValue& target) const
{
    Assert(cls != nullptr);
    Assert(target.IsValid());

    HashSet<const IMember*> assetReferenceTargetMembersMap;
    HashMap<const IMember*, const IMember*> assetReferenceMembersMap;

    { // collect asset reference members
        Array<Pair<const IMember*, const IMember*>> assetReferenceMembers;
        CollectAssetReferenceMembers(cls, assetReferenceMembers);

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
        HYP_NAMED_SCOPE_FMT("Deserializing properties for Class '{}'", cls->GetName());

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
        GlobalContextScope contextScope { LoadAssetsFromReferencesContext {} };
#endif

        for (const KeyValuePair<ANSIString, FBOMData>& it : in.GetProperties())
        {
            if (const IMember* pMember = cls->GetMember(it.first, MemberType::Property | MemberType::Field))
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
                    HYP_NAMED_SCOPE_FMT("Deserializing member '{}' on object, skipping setter for Class '{}'", pMember->GetName(), cls->GetName());

                    HYP_LOG(Serialization, Warning, "Member '{}' of Class '{}' is not deserializable, skipping", pMember->GetName(), cls->GetName());

                    continue;
                }

                if (Result deserializeResult = pMember->Deserialize(context, target, it.second); deserializeResult.HasError())
                {
                    return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to deserialize member '{}' of Class '{}': {}", pMember->GetName(), cls->GetName(), deserializeResult.GetError().GetMessage()) };
                }
            }
            else
            {
                HYP_LOG(Serialization, Warning, "No member named '{}' found in Class '{}', skipping", it.first, cls->GetName());
            }
        }
    }

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
    if (IsGlobalContextActive<LoadAssetsFromReferencesContext>() && target.GetTypeInfo()->GetClass() == AssetReference::StaticClass())
    {
        // resolve asset reference to asset object
        AssetReference& assetReference = target.Get<AssetReference>();
        const Handle<AssetObject>& assetObject = ResolveAssetImpl(assetReference);

        if (assetObject)
        {
            target = BoxedValue(assetObject);

            return {};
        }

        HYP_LOG(Serialization, Warning, "Failed to resolve AssetReference for asset '{}'", assetReference.GetAssetPath().ToString());

        return {};
    }
#endif

    cls->PostLoad(target.ToRef().GetPointer());

    return { FBOMResult::FBOM_OK };
}

} // namespace Hyperion::serialization
