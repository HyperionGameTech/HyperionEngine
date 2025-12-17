/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>

namespace hyperion::serialization {

class NodeMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        const Node& inObject = in.Get<Node>();

        HYP_LOG(Serialization, Debug, "Serializing Node with name '{}'...", inObject.GetName());

        if (inObject.GetNodeFlags() & NodeFlags::TRANSIENT)
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize Node: TRANSIENT flag is set" };
        }

        if (FBOMResult err = ObjectMarshal::Serialize(in, out))
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
            if (!child.IsValid() || (child->GetNodeFlags() & NodeFlags::TRANSIENT))
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
        NodeTagSet tags;

        if (FBOMResult err = HypData::Deserialize(context, in.GetProperty("Tags"), tags))
        {
            return err;
        }

        const Class* cls = in.GetClass();

        if (!cls)
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Object {} does not have a Class defined", in.GetType().ToString()) };
        }

        if (!cls->IsDerivedFrom(Node::StaticClass()))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Class {} is not derived from Node", cls->GetName()) };
        }

        if (!cls->CreateInstance(out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to create instance of Class {}", cls->GetName()) };
        }

        if (FBOMResult err = ObjectMarshal::Deserialize_Internal(context, in, cls, out))
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

HYP_DEFINE_MARSHAL(Node, NodeMarshal);

} // namespace hyperion::serialization
