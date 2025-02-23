/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Node> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject &out) const override
    {
        const Node &in_object = in.Get<Node>();

        HYP_LOG(Serialization, Debug, "Serializing Node with name '{}'...", in_object.GetName());

        if (in_object.GetFlags() & NodeFlags::TRANSIENT) {
            return { FBOMResult::FBOM_ERR, "Cannot serialize Node: TRANSIENT flag is set" };
        }

        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out)) {
            return err;
        }

        out.SetProperty("Type", uint32(in_object.GetType()));

        {
            FBOMData tags_data;

            if (FBOMResult err = HypData::Serialize(in_object.GetTags(), tags_data)) {
                return err;
            }

            out.SetProperty("Tags", std::move(tags_data));
        }

        for (const NodeProxy &child : in_object.GetChildren()) {
            if (!child.IsValid() || (child->GetFlags() & NodeFlags::TRANSIENT)) {
                continue;
            }

            if (FBOMResult err = out.AddChild(*child.Get(), FBOMObjectSerializeFlags::KEEP_UNIQUE)) {
                return err;
            }
        }

        HYP_LOG(Serialization, Debug, "Serialization completed for Node with name '{}'", in_object.GetName());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        Node::Type node_type = Node::Type::NODE;

        if (FBOMResult err = in.GetProperty("Type").ReadUInt32(&node_type)) {
            return err;
        }

        Node::NodeTagSet tags;

        if (FBOMResult err = HypData::Deserialize(in.GetProperty("Tags"), tags)) {
            return err;
        }

        NodeProxy node;

        switch (node_type) {
        case Node::Type::NODE:
            node = NodeProxy(MakeRefCountedPtr<Node>());
            break;
        case Node::Type::BONE:
            node = NodeProxy(MakeRefCountedPtr<Bone>());

            break;
        default:
            return { FBOMResult::FBOM_ERR, "Unsupported node type" }; 
        }

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(in, node->InstanceClass(), AnyRef(*node))) {
            return err;
        }
        
        for (NodeTag &tag : tags) {
            node->AddTag(std::move(tag));
        }

        for (FBOMObject &subobject : *in.nodes) {
            if (subobject.GetType().IsOrExtends("Node")) {
                node->AddChild(subobject.m_deserialized_object->Get<NodeProxy>());
            }
        }

        out = HypData(std::move(node));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Node, FBOMMarshaler<Node>);

} // namespace hyperion::fbom