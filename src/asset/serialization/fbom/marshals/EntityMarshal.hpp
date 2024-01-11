#ifndef HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_ENTITY_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/MeshMarshal.hpp>
#include <asset/serialization/fbom/marshals/ShaderMarshal.hpp>
#include <asset/serialization/fbom/marshals/MaterialMarshal.hpp>
#include <asset/serialization/fbom/marshals/ControllerMarshal.hpp>
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
        return FBOMObjectType(Entity::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Entity &in_object, FBOMObject &out) const override
    {
        out.SetProperty("name", FBOMName(), in_object.GetName());
    
        out.SetProperty("transform.translation", FBOMVec3f(), &in_object.GetTransform().GetTranslation());
        out.SetProperty("transform.rotation", FBOMQuaternion(), &in_object.GetTransform().GetRotation());
        out.SetProperty("transform.scale", FBOMVec3f(), &in_object.GetTransform().GetScale());

        out.SetProperty("local_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetLocalAABB());
        out.SetProperty("world_aabb", FBOMStruct(sizeof(BoundingBox)), &in_object.GetWorldAABB());

        if (const auto &mesh = in_object.GetMesh()) {
            out.AddChild(*mesh, FBOM_OBJECT_FLAGS_EXTERNAL);
        }

        if (const auto &material = in_object.GetMaterial()) {
            out.AddChild(*material, FBOM_OBJECT_FLAGS_EXTERNAL);
        }

        for (auto it = g_engine->GetComponents().Begin(in_object.GetID()); it != g_engine->GetComponents().End(in_object.GetID()); ++it) {
            out.AddChild(*it, FBOM_OBJECT_FLAGS_KEEP_UNIQUE);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto entity_handle = UniquePtr<Handle<Entity>>::Construct(CreateObject<Entity>());

        Name name;
        in.GetProperty("name").ReadName(&name);

        (*entity_handle)->SetName(name);

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
            (*entity_handle)->SetTransform(transform);
        }

        { // bounding box
            BoundingBox local_aabb = BoundingBox::empty;
            BoundingBox world_aabb = BoundingBox::empty;

            if (auto err = in.GetProperty("local_aabb").ReadStruct(sizeof(BoundingBox), &local_aabb)) {
                return err;
            }

            (*entity_handle)->SetLocalAABB(local_aabb);

            if (auto err = in.GetProperty("world_aabb").ReadStruct(sizeof(BoundingBox), &world_aabb)) {
                return err;
            }

            (*entity_handle)->SetWorldAABB(world_aabb);
        }

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Mesh")) {
                (*entity_handle)->SetMesh(node.deserialized.Get<Mesh>());
            } else if (node.GetType().IsOrExtends("Material")) {
                (*entity_handle)->SetMaterial(node.deserialized.Get<Material>());
            } else if (node.GetType().IsOrExtends("Controller")) {
                if (auto wrapper = node.deserialized.Get<ControllerSerializationWrapper>()) {
                    if (wrapper->controller) {
                        g_engine->GetComponents().Add(*entity_handle, wrapper->type_id, std::move(wrapper->controller));
                    } else {
                        DebugLog(
                            LogType::Warn,
                            "Invalid controller on serialized Entity, the controller is no longer set and probably had its ownership taken by another Entity.\n"
                            "Make sure serialized controllers have a unique ID, as they are not shared between Entities.\n"
                        );
                    }
                }
            }
        }

        (*entity_handle)->RebuildRenderableAttributes();

        out_object = std::move(entity_handle);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif