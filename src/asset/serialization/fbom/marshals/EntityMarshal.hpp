#ifndef HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/MeshMarshal.hpp>
#include <asset/serialization/fbom/marshals/ShaderMarshal.hpp>
#include <asset/serialization/fbom/marshals/MaterialMarshal.hpp>
#include <scene/Entity.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Entity> : public FBOMObjectMarshalerBase<Entity>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("Entity");
    }

    virtual FBOMResult Serialize(const Entity &in_object, FBOMObject &out) const override
    {
        out.SetProperty("transform.translation", FBOMVec3f(), &in_object.GetTransform().GetTranslation());
        out.SetProperty("transform.rotation", FBOMQuaternion(), &in_object.GetTransform().GetRotation());
        out.SetProperty("transform.scale", FBOMVec3f(), &in_object.GetTransform().GetScale());

        out.SetProperty("local_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetLocalAABB());
        out.SetProperty("world_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetWorldAABB());

        // out.SetProperty("renderable_attributes.bucket", FBOMUnsignedInt(), in_object.GetRenderableAttributes().bucket);
        // out.SetProperty("renderable_attributes.vertex_attributes", FBOMStruct(sizeof(VertexAttributeSet)), &in_object.GetRenderableAttributes().vertex_attributes);
        // out.SetProperty("renderable_attributes.topology", FBOMUnsignedInt(), in_object.GetRenderableAttributes().topology);
        // out.SetProperty("renderable_attributes.fill_mode", FBOMUnsignedInt(), in_object.GetRenderableAttributes().fill_mode);
        // out.SetProperty("renderable_attributes.cull_faces", FBOMUnsignedInt(), in_object.GetRenderableAttributes().cull_faces);
        // out.SetProperty("renderable_attributes.alpha_blending", FBOMBool(), in_object.GetRenderableAttributes().alpha_blending);
        // out.SetProperty("renderable_attributes.depth_write", FBOMBool(), in_object.GetRenderableAttributes().depth_write);
        // out.SetProperty("renderable_attributes.depth_test", FBOMBool(), in_object.GetRenderableAttributes().depth_test);

        if (const auto &mesh = in_object.GetMesh()) {
            out.AddChild(*mesh, false);
        }

        if (const auto &shader = in_object.GetShader()) {
            out.AddChild(*shader, false);
        }

        if (const auto &material = in_object.GetMaterial()) {
            out.AddChild(*material, false);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(Engine *engine, const FBOMObject &in, Entity *&out_object) const override
    {
        out_object = new Entity();

        { // transform
            Transform transform = Transform::identity;

            if (auto err = in.GetProperty("transform.translation").ReadArrayElements(FBOMFloat(), 3, &transform.GetTranslation())) {
                return err;
            }
        
            if (auto err = in.GetProperty("transform.rotation").ReadArrayElements(FBOMFloat(), 4, &transform.GetRotation())) {
                return err;
            }

            if (auto err = in.GetProperty("transform.scale").ReadArrayElements(FBOMFloat(), 3, &transform.GetScale())) {
                return err;
            }

            transform.UpdateMatrix();
            out_object->SetTransform(transform);
        }

        { // bounding box
            BoundingBox local_aabb = BoundingBox::empty;
            BoundingBox world_aabb = BoundingBox::empty;

            if (auto err = in.GetProperty("local_aabb").ReadStruct(sizeof(BoundingBox), &local_aabb)) {
                return err;
            }

            out_object->SetLocalAABB(local_aabb);

            if (auto err = in.GetProperty("world_aabb").ReadStruct(sizeof(BoundingBox), &world_aabb)) {
                return err;
            }

            out_object->SetWorldAABB(world_aabb);
        }

        RenderableAttributeSet renderable_attributes;

        // in.GetProperty("renderable_attributes.bucket").ReadUnsignedInt(&renderable_attributes.bucket);
        // in.GetProperty("renderable_attributes.vertex_attributes").ReadStruct(&renderable_attributes.vertex_attributes);
        // in.GetProperty("renderable_attributes.topology").ReadUnsignedInt(&renderable_attributes.topology);
        // in.GetProperty("renderable_attributes.fill_mode").ReadUnsignedInt(&renderable_attributes.fill_mode);
        // in.GetProperty("renderable_attributes.cull_faces").ReadUnsignedInt(&renderable_attributes.cull_faces);
        // in.GetProperty("renderable_attributes.alpha_blending").ReadBool(&renderable_attributes.alpha_blending);
        // in.GetProperty("renderable_attributes.depth_write").ReadBool(&renderable_attributes.depth_write);
        // in.GetProperty("renderable_attributes.depth_test").ReadBool(&renderable_attributes.depth_test);

        out_object->SetRenderableAttributes(renderable_attributes);

        for (auto &node : *in.nodes) {
            // TODO needs to attach to engine to get ID
            if (node.GetType().IsOrExtends("Mesh")) {
                out_object->SetMesh(engine->CreateHandle<Mesh>(node.deserialized.Cast<Mesh>()));
            } else if (node.GetType().IsOrExtends("Shader")) {
                out_object->SetShader(engine->CreateHandle<Shader>(node.deserialized.Cast<Shader>()));
            } else if (node.GetType().IsOrExtends("Material")) {
                out_object->SetMaterial(engine->CreateHandle<Material>(node.deserialized.Cast<Material>()));
            }
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif