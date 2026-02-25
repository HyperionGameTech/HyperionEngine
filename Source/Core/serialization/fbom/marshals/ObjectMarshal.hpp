/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Util.hpp>

#include <Core/serialization/fbom/FBOM.hpp>
#include <Core/serialization/fbom/FBOMMarshaler.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/reflection/TypeInfoFwd.hpp>

#include <Core/Constants.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <asset/AssetReference.hpp>
#endif

namespace Hyperion {

class ObjectBase;
struct BoxedValue;

template <class T>
class ClassInstance;

// Leave implementation empty - stub class
template <>
class ClassInstance<void>;

using ClassStub = ClassInstance<void>;

#if !defined(HYPERION_ENGINE) || !HYPERION_ENGINE

class AssetReference;
class AssetObject;

/// Stub implementation when HYPERION_ENGINE is not defined
static inline const Handle<AssetObject>& ResolveAssetImpl(const AssetReference& assetReference)
{
    static const Handle<AssetObject> s_emptyHandle;
    HYP_BREAKPOINT;
    return s_emptyHandle;
}

#else

struct EditorProjectSaveContext;

extern const Handle<AssetObject>& ResolveAssetImpl(const AssetReference& assetReference);

#endif

} // namespace Hyperion

namespace Hyperion::serialization {

class HYP_API ObjectMarshal : public FBOMMarshalerBase
{
public:
    virtual ~ObjectMarshal() override = default;

    virtual FBOMType GetObjectType() const override;
    virtual TypeId GetTypeId() const override final;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override;

protected:
    /*! \brief Deserialize into an existing object.
     *
     *  \param in The FBOMObject to deserialize.
     *  \param cls The Class of the instance.
     *  \param ref The instance to deserialize into.
     *  \return The result of the deserialization.
     */
    virtual FBOMResult Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const Class* cls, BoxedValue& target) const;
};

} // namespace Hyperion::serialization
