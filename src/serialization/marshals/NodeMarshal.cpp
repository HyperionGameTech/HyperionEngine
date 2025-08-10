/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<Node> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        const Node& inObject = in.Get<Node>();

        HYP_LOG(Serialization, Debug, "Serializing Node with name '{}'...", inObject.GetName());

        if (inObject.GetFlags() & NodeFlags::TRANSIENT)
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize Node: TRANSIENT flag is set" };
        }

        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        {
            FBOMData tagsData;

            if (FBOMResult err = HypData::Serialize(inObject.GetTags(), tagsData))
            {
                return err;
            }

            out.SetProperty("Tags", std::move(tagsData));
        }

        for (const Handle<Node>& child : inObject.GetChildren())
        {
            if (!child.IsValid() || (child->GetFlags() & NodeFlags::TRANSIENT))
            {
                continue;
            }

            if (FBOMResult err = out.AddChild(*child.Get(), FBOMObjectSerializeFlags::KEEP_UNIQUE))
            {
                return err;
            }
        }

        HYP_LOG(Serialization, Debug, "Serialization completed for Node with name '{}'", inObject.GetName());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Node::NodeTagSet tags;

        if (FBOMResult err = HypData::Deserialize(context, in.GetProperty("Tags"), tags))
        {
            return err;
        }

        const HypClass* hypClass = in.GetHypClass();

        if (!hypClass)
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Object {} does not have a HypClass defined", in.GetType().ToString()) };
        }

        if (!hypClass->IsDerivedFrom(Node::Class()))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("HypClass {} is not derived from Node", hypClass->GetName()) };
        }

        if (!hypClass->CreateInstance(out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to create instance of HypClass {}", hypClass->GetName()) };
        }

        HYP_LOG(Serialization, Debug, "Deserializing Node of type: {}", hypClass->GetName());

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, hypClass, out.ToRef()))
        {
            return err;
        }

        const Handle<Node>& node = out.Get<Handle<Node>>();
        Assert(node != nullptr, "Deserialized HypData is not a valid Node handle");

        for (NodeTag& tag : tags)
        {
            node->AddTag(std::move(tag));
        }

        for (const FBOMObject& child : in.GetChildren())
        {
            if (child.GetType().IsOrExtends("Node"))
            {
                node->AddChild(child.m_deserializedObject->Get<Handle<Node>>());
            }
        }

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Node, FBOMMarshaler<Node>);

} // namespace hyperion::serialization