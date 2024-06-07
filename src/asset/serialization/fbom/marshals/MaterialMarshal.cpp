/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Material.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Material> : public FBOMObjectMarshalerBase<Material>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Material &in_object, FBOMObject &out) const override
    {
        out.SetProperty("name", FBOMName(), in_object.GetName());

        out.SetProperty("attributes.bucket", FBOMUnsignedInt(), uint32(in_object.GetRenderAttributes().bucket));
        out.SetProperty("attributes.flags", FBOMUnsignedInt(), uint32(in_object.GetRenderAttributes().flags));
        out.SetProperty("attributes.cull_mode", FBOMUnsignedInt(), uint32(in_object.GetRenderAttributes().cull_faces));
        out.SetProperty("attributes.fill_mode", FBOMUnsignedInt(), uint32(in_object.GetRenderAttributes().fill_mode));

        out.SetProperty("params.size", FBOMUnsignedInt(), uint32(in_object.GetParameters().Size()));

        for (SizeType i = 0; i < in_object.GetParameters().Size(); i++) {
            const auto key_value = in_object.GetParameters().KeyValueAt(i);

            out.SetProperty(
                String("params.") + String::ToString(uint(key_value.first)) + ".key",
                FBOMUnsignedLong(),
                key_value.first
            );

            out.SetProperty(
                String("params.") + String::ToString(uint(key_value.first)) + ".type",
                FBOMUnsignedInt(),
                key_value.second.type
            );

            if (key_value.second.IsIntType()) {
                for (uint j = 0; j < 4; j++) {
                    out.SetProperty(
                        String("params.") + String::ToString(uint(key_value.first)) + ".values[" + String::ToString(j) + "]",
                        FBOMInt(),
                        key_value.second.values.int_values[j]
                    );
                }
            } else if (key_value.second.IsFloatType()) {
                for (uint j = 0; j < 4; j++) {
                    out.SetProperty(
                        String("params.") + String::ToString(uint(key_value.first)) + ".values[" + String::ToString(j) + "]",
                        FBOMFloat(),
                        key_value.second.values.float_values[j]
                    );
                }
            }
        }

        uint32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        for (SizeType i = 0, texture_index = 0; i < in_object.GetTextures().Size(); i++) {
            const auto key = in_object.GetTextures().KeyAt(i);
            const auto &value = in_object.GetTextures().ValueAt(i);

            if (value) {
                if (auto err = out.AddChild(*value, FBOM_OBJECT_FLAGS_EXTERNAL)) {
                    return err;
                }

                texture_keys[texture_index++] = uint32(key);
            }
        }

        if (const ShaderRef &shader = in_object.GetShader()) {
            if (shader.IsValid()) {
                // @TODO serialize the shader
            }
        }

        out.SetProperty(
            "texture_keys",
            FBOMSequence(FBOMUnsignedInt(), std::size(texture_keys)),
            &texture_keys[0]
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        Name name;
        in.GetProperty("name").ReadName(&name);

        MaterialAttributes attributes;
        Material::ParameterTable parameters = Material::DefaultParameters();
        Material::TextureSet textures;

        in.GetProperty("attributes.bucket").ReadUnsignedInt(&attributes.bucket);
        in.GetProperty("attributes.flags").ReadUnsignedInt(&attributes.flags);
        in.GetProperty("attributes.cull_mode").ReadUnsignedInt(&attributes.cull_faces);
        in.GetProperty("attributes.fill_mode").ReadUnsignedInt(&attributes.fill_mode);

        uint32 num_parameters;

        // load parameters
        if (auto err = in.GetProperty("params.size").ReadUnsignedInt(&num_parameters)) {
            return err;
        }

        for (uint i = 0; i < num_parameters; i++) {
            const auto param_string = String("params.") + String::ToString(i);

            Material::MaterialKey key;
            Material::Parameter param;

            if (auto err = in.GetProperty(param_string).ReadElements(FBOMFloat(), 4, &param.values)) {
                continue;
            }

            if (auto err = in.GetProperty(param_string + ".key").ReadUnsignedLong(&key)) {
                continue;
            }

            if (auto err = in.GetProperty(param_string + ".type").ReadUnsignedInt(&param.type)) {
                continue;
            }

            if (param.IsIntType()) {
                for (uint j = 0; j < 4; j++) {
                    in.GetProperty(param_string + ".values[" + String::ToString(j) + "]")
                        .ReadInt(&param.values.int_values[j]);
                }
            } else if (param.IsFloatType()) {
                for (uint j = 0; j < 4; j++) {
                    in.GetProperty(param_string + ".values[" + String::ToString(j) + "]")
                        .ReadFloat(&param.values.float_values[j]);
                }
            }

            parameters.Set(key, param);
        }

        uint32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        if (auto err = in.GetProperty("texture_keys").ReadElements(FBOMUnsignedInt(), std::size(texture_keys), &texture_keys[0])) {
            return err;
        }

        uint texture_index = 0;

        ShaderRef shader = g_shader_manager->GetOrCreate(
            HYP_NAME(Forward),
            ShaderProperties()
        );

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Texture")) {
                if (texture_index < std::size(texture_keys)) {
                    if (auto texture = node.deserialized.Get<Texture>()) {
                        textures.Set(
                            Material::TextureKey(texture_keys[texture_index]), 
                            std::move(texture)
                        );

                        ++texture_index;
                    }
                }
            }
        }

        Handle<Material> material_handle = g_material_system->GetOrCreate(attributes, parameters, textures);
        material_handle->SetShader(std::move(shader));

        if (name) {
            material_handle->SetName(name);
        }

        out_object = std::move(material_handle);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Material, FBOMMarshaler<Material>);

} // namespace hyperion::fbom