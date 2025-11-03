/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Util.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

#include <core/reflection/Handle.hpp>

#include <core/reflection/TypeInfoFwd.hpp>

#include <core/Constants.hpp>

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE
#include <asset/AssetReference.hpp>
#endif

namespace hyperion {

class HypObjectBase;
struct HypData;

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
    HYP_BREAKPOINT_DEBUG_MODE;
    return s_emptyHandle;
}

#else

struct EditorProjectSaveContext;

extern const Handle<AssetObject>& ResolveAssetImpl(const AssetReference& assetReference);

#endif

} // namespace hyperion

namespace hyperion::serialization {

class HYP_API ObjectMarshal : public FBOMMarshalerBase
{
public:
    virtual ~ObjectMarshal() override = default;

    virtual FBOMType GetObjectType() const override;
    virtual TypeId GetTypeId() const override final;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override;

protected:
    /*! \brief Deserialize into an existing object.
     *
     *  \param in The FBOMObject to deserialize.
     *  \param cls The Class of the instance.
     *  \param ref The instance to deserialize into.
     *  \return The result of the deserialization.
     */
    virtual FBOMResult Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const Class* cls, HypData& target) const;
};

} // namespace hyperion::serialization
