/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMArray.hpp>

#include <rendering/Material.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Material> : public FBOMObjectMarshalerBase<Material>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Material &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("name"), FBOMData::FromName(in_object.GetName()));
        out.SetProperty(NAME("attributes"), FBOMData::FromObject(
            FBOMObject()
                .SetProperty(NAME("bucket"), uint32(in_object.GetRenderAttributes().bucket))
                .SetProperty(NAME("flags"), uint32(in_object.GetRenderAttributes().flags))
                .SetProperty(NAME("cull_mode"), uint32(in_object.GetRenderAttributes().cull_faces))
                .SetProperty(NAME("fill_mode"), uint32(in_object.GetRenderAttributes().fill_mode))
        ));

        FBOMArray params_array;

        for (SizeType i = 0; i < in_object.GetParameters().Size(); i++) {
            const auto key_value = in_object.GetParameters().KeyValueAt(i);
            
            FBOMObject param_object;
            param_object.SetProperty(NAME("key"), uint64(key_value.first));
            param_object.SetProperty(NAME("type"), uint32(key_value.second.type));

            if (key_value.second.IsIntType()) {
                param_object.SetProperty(NAME("data"), FBOMSequence(FBOMInt(), ArraySize(key_value.second.values.int_values)), &key_value.second.values.int_values[0]); 
            } else if (key_value.second.IsFloatType()) {
                param_object.SetProperty(NAME("data"), FBOMSequence(FBOMFloat(), ArraySize(key_value.second.values.float_values)), &key_value.second.values.float_values[0]); 
            }

            params_array.AddElement(FBOMData::FromObject(std::move(param_object)));
        }
        
        out.SetProperty(NAME("params"), FBOMData::FromArray(std::move(params_array)));

        uint32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        for (SizeType i = 0, texture_index = 0; i < in_object.GetTextures().Size(); i++) {
            const auto key = in_object.GetTextures().KeyAt(i);
            const auto &value = in_object.GetTextures().ValueAt(i);

            if (value) {
                if (FBOMResult err = out.AddChild(*value, FBOMObjectFlags::EXTERNAL)) {
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
            NAME("texture_keys"),
            FBOMSequence(FBOMUnsignedInt(), ArraySize(texture_keys)),
            &texture_keys[0]
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        Name name;
        if (FBOMResult err = in.GetProperty("name").ReadName(&name)) {
            return err;
        }

        MaterialAttributes attributes;
        Material::ParameterTable parameters = Material::DefaultParameters();
        Material::TextureSet textures;

        FBOMObject attributes_object;

        if (FBOMResult err = in.GetProperty("attributes").ReadObject(attributes_object)) {
            return err;
        }

        attributes_object.GetProperty("bucket").ReadUnsignedInt(&attributes.bucket);
        attributes_object.GetProperty("flags").ReadUnsignedInt(&attributes.flags);
        attributes_object.GetProperty("cull_mode").ReadUnsignedInt(&attributes.cull_faces);
        attributes_object.GetProperty("fill_mode").ReadUnsignedInt(&attributes.fill_mode);

        FBOMArray params_array;

        if (FBOMResult err = in.GetProperty("params").ReadArray(params_array)) {
            return err;
        }

        for (SizeType i = 0; i < params_array.Size(); i++) {
            Material::Parameter param;

            if (const FBOMData &element = params_array.GetElement(i)) {
                FBOMObject param_object;

                if (FBOMResult err = element.ReadObject(param_object)) {
                    return err;
                }

                Material::MaterialKey param_key;

                if (FBOMResult err = param_object.GetProperty("key").ReadUnsignedLong(&param_key)) {
                    return err;
                }

                if (FBOMResult err = param_object.GetProperty("type").ReadUnsignedInt(&param.type)) {
                    return err;
                }

                if (param.IsIntType()) {
                    if (FBOMResult err = param_object.GetProperty("data").ReadElements(FBOMInt(), ArraySize(param.values.int_values), &param.values.int_values[0])) {
                        return err;
                    }
                } else if (param.IsFloatType()) {
                    if (FBOMResult err = param_object.GetProperty("data").ReadElements(FBOMFloat(), ArraySize(param.values.float_values), &param.values.float_values[0])) {
                        return err;
                    }
                }

                parameters.Set(param_key, param);
            }
        }

        uint32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        if (FBOMResult err = in.GetProperty("texture_keys").ReadElements(FBOMUnsignedInt(), std::size(texture_keys), &texture_keys[0])) {
            return err;
        }

        uint32 texture_index = 0;

        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("Forward"),
            ShaderProperties()
        );

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Texture")) {
                if (texture_index < std::size(texture_keys)) {
                    if (Handle<Texture> texture = node.m_deserialized_object->Get<Texture>()) {
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