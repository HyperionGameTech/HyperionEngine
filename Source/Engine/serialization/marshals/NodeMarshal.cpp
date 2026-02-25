/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/serialization/fbom/FBOMMarshaler.hpp>
#include <Core/serialization/fbom/FBOMData.hpp>
#include <Core/serialization/fbom/FBOMObject.hpp>
#include <Core/serialization/fbom/marshals/ObjectMarshal.hpp>

#include <scene/Node.hpp>
#include <scene/animation/Bone.hpp>

namespace Hyperion::serialization {

class NodeMarshal : public ObjectMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        const Node& inObject = in.Get<Node>();

        HYP_LOG(Serialization, Verbose, "Serializing Node with name '{}'...", inObject.GetName());

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

            if (FBOMResult err = BoxedValue::Serialize(inObject.GetTags(), tagsData))
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

        HYP_LOG(Serialization, Verbose, "Serialization completed for Node with name '{}'", inObject.GetName());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override
    {
        NodeTagSet tags;

        if (FBOMResult err = BoxedValue::Deserialize(context, in.GetProperty("Tags"), tags))
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
        Assert(node != nullptr, "Deserialized BoxedValue is not a valid Node handle");

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

} // namespace Hyperion::serialization
