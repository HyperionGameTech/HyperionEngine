/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Node> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject &out) const override
    {
        const Node &in_object = in.Get<Node>();

        HYP_LOG(Serialization, LogLevel::DEBUG, "Serializing Node with name '{}'...", in_object.GetName());

        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out)) {
            return err;
        }

        out.SetProperty("Type", uint32(in_object.GetType()));

        for (const NodeProxy &child : in_object.GetChildren()) {
            if (!child.IsValid()) {
                continue;
            }

            if (FBOMResult err = out.AddChild(*child.Get(), FBOMObjectSerializeFlags::KEEP_UNIQUE)) {
                return err;
            }
        }

        HYP_LOG(Serialization, LogLevel::DEBUG, "Serialization completed for Node with name '{}'", in_object.GetName());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        Node::Type node_type = Node::Type::NODE;

        if (FBOMResult err = in.GetProperty("Type").ReadUInt32(&node_type)) {
            return err;
        }

        NodeProxy node;
        
        const HypClass *node_hyp_class = Node::GetClass();

        switch (node_type) {
        case Node::Type::NODE:
            node = NodeProxy(new Node());
            break;
        case Node::Type::BONE:
            node = NodeProxy(new Bone());

            node_hyp_class = Bone::GetClass();

            break;
        default:
            return { FBOMResult::FBOM_ERR, "Unsupported node type" }; 
        }

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(in, node_hyp_class, AnyRef(*node))) {
            return err;
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