/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMArray.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <scene/Material.hpp>

#include <rendering/backend/RendererShader.hpp>

#include <HyperionEngine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Material> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject &out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out)) {
            return err;
        }

        const Material &in_object = in.Get<Material>();

        out.SetProperty("Attributes", FBOMData::FromObject(
            FBOMObject()
                .SetProperty("Bucket", uint32(in_object.GetRenderAttributes().bucket))
                .SetProperty("Flags", uint32(in_object.GetRenderAttributes().flags))
                .SetProperty("CullMode", uint32(in_object.GetRenderAttributes().cull_faces))
                .SetProperty("FillMode", uint32(in_object.GetRenderAttributes().fill_mode))
        ));

        FBOMArray params_array { FBOMBaseObjectType() };

        for (SizeType i = 0; i < in_object.GetParameters().Size(); i++) {
            const auto key_value = in_object.GetParameters().KeyValueAt(i);
            
            FBOMObject param_object;
            param_object.SetProperty("Key", uint64(key_value.first));
            param_object.SetProperty("Type", uint32(key_value.second.type));

            if (key_value.second.IsIntType()) {
                param_object.SetProperty("Data", FBOMSequence(FBOMInt32(), ArraySize(key_value.second.values.int_values)), ArraySize(key_value.second.values.int_values) * sizeof(int32), &key_value.second.values.int_values[0]); 
            } else if (key_value.second.IsFloatType()) {
                param_object.SetProperty("Data", FBOMSequence(FBOMFloat(), ArraySize(key_value.second.values.float_values)), ArraySize(key_value.second.values.int_values) * sizeof(float), &key_value.second.values.float_values[0]); 
            }

            params_array.AddElement(FBOMData::FromObject(std::move(param_object)));
        }
        
        out.SetProperty("Parameters", FBOMData::FromArray(std::move(params_array)));

        uint32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        // for (SizeType i = 0, texture_index = 0; i < in_object.GetTextures().Size(); i++) {
        //     const Material::TextureKey key = in_object.GetTextures().KeyAt(i);
        //     const Handle<Texture> &value = in_object.GetTextures().ValueAt(i);

        //     if (value) {
        //         if (FBOMResult err = out.AddChild(*value)) {
        //             return err;
        //         }

        //         texture_keys[texture_index++] = uint32(key);
        //     }
        // }

        if (const ShaderRef &shader = in_object.GetShader()) {
            if (shader.IsValid()) {
                // @TODO serialize the shader
            }
        }

        out.SetProperty(
            "TextureKeys",
            FBOMSequence(FBOMUInt32(), ArraySize(texture_keys)),
            ArraySize(texture_keys) * sizeof(uint32),
            &texture_keys[0]
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        MaterialAttributes attributes;
        Material::ParameterTable parameters = Material::DefaultParameters();
        Material::TextureSet textures;

        FBOMObject attributes_object;

        if (FBOMResult err = in.GetProperty("Attributes").ReadObject(attributes_object)) {
            return err;
        }

        attributes_object.GetProperty("Bucket").ReadUInt32(&attributes.bucket);
        attributes_object.GetProperty("Flags").ReadUInt32(&attributes.flags);
        attributes_object.GetProperty("CullMode").ReadUInt32(&attributes.cull_faces);
        attributes_object.GetProperty("FillMode").ReadUInt32(&attributes.fill_mode);

        FBOMArray params_array { FBOMUnset() };

        if (FBOMResult err = in.GetProperty("Parameters").ReadArray(params_array)) {
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

                if (FBOMResult err = param_object.GetProperty("Key").ReadUInt64(&param_key)) {
                    return err;
                }

                if (FBOMResult err = param_object.GetProperty("Type").ReadUInt32(&param.type)) {
                    return err;
                }

                if (param.IsIntType()) {
                    if (FBOMResult err = param_object.GetProperty("Data").ReadElements(FBOMInt32(), ArraySize(param.values.int_values), &param.values.int_values[0])) {
                        return err;
                    }
                } else if (param.IsFloatType()) {
                    if (FBOMResult err = param_object.GetProperty("Data").ReadElements(FBOMFloat(), ArraySize(param.values.float_values), &param.values.float_values[0])) {
                        return err;
                    }
                }

                parameters.Set(param_key, param);
            }
        }

        uint32 texture_keys[Material::max_textures];
        Memory::MemSet(&texture_keys[0], 0, sizeof(texture_keys));

        if (FBOMResult err = in.GetProperty("TextureKeys").ReadElements(FBOMUInt32(), std::size(texture_keys), &texture_keys[0])) {
            return err;
        }

        uint32 texture_index = 0;

        ShaderRef shader = g_shader_manager->GetOrCreate(
            NAME("Forward"),
            ShaderProperties(static_mesh_vertex_attributes)
        );

        for (auto &subobject : *in.nodes) {
            if (subobject.GetType().IsOrExtends("Texture")) {
                if (texture_index < std::size(texture_keys)) {
                    if (Optional<const Handle<Texture> &> texture_opt = subobject.m_deserialized_object->TryGet<Handle<Texture>>()) {
                        textures.Set(
                            MaterialTextureKey(texture_keys[texture_index]), 
                            *texture_opt
                        );

                        ++texture_index;
                    }
                }
            }
        }

        Handle<Material> material_handle = g_material_system->GetOrCreate(attributes, parameters, textures);
        material_handle->SetShader(std::move(shader));

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(in, Material::Class(), AnyRef(*material_handle))) {
            return err;
        }

        out = HypData(std::move(material_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Material, FBOMMarshaler<Material>);

} // namespace hyperion::fbom