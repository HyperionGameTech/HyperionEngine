#ifndef HYPERION_V2_FBOM_MARSHALS_MATERIAL_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_MATERIAL_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/TextureMarshal.hpp>
#include <rendering/Material.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Material> : public FBOMObjectMarshalerBase<Material>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(Material::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Material &in_object, FBOMObject &out) const override
    {
        out.SetProperty("name", FBOMString(), in_object.GetName().Size(), in_object.GetName().Data());

        out.SetProperty("attributes.bucket", FBOMUnsignedInt(), static_cast<UInt32>(in_object.GetRenderAttributes().bucket));
        out.SetProperty("attributes.flags", FBOMUnsignedInt(), static_cast<UInt32>(in_object.GetRenderAttributes().flags));
        out.SetProperty("attributes.cull_mode", FBOMUnsignedInt(), static_cast<UInt32>(in_object.GetRenderAttributes().cull_faces));
        out.SetProperty("attributes.fill_mode", FBOMUnsignedInt(), static_cast<UInt32>(in_object.GetRenderAttributes().fill_mode));

        out.SetProperty("params.size", FBOMUnsignedInt(), static_cast<UInt32>(in_object.GetParameters().Size()));

        for (SizeType i = 0; i < in_object.GetParameters().Size(); i++) {
            const auto key_value = in_object.GetParameters().KeyValueAt(i);

            out.SetProperty(
                String("params.") + String::ToString(static_cast<UInt>(key_value.first)) + ".key",
                FBOMUnsignedLong(),
                key_value.first
            );

            out.SetProperty(
                String("params.") + String::ToString(static_cast<UInt>(key_value.first)) + ".type",
                FBOMUnsignedInt(),
                key_value.second.type
            );

            if (key_value.second.IsIntType()) {
                for (UInt j = 0; j < 4; j++) {
                    out.SetProperty(
                        String("params.") + String::ToString(static_cast<UInt>(key_value.first)) + ".values[" + String::ToString(j) + "]",
                        FBOMInt(),
                        key_value.second.values.int_values[j]
                    );
                }
            } else if (key_value.second.IsFloatType()) {
                for (UInt j = 0; j < 4; j++) {
                    out.SetProperty(
                        String("params.") + String::ToString(static_cast<UInt>(key_value.first)) + ".values[" + String::ToString(j) + "]",
                        FBOMFloat(),
                        key_value.second.values.float_values[j]
                    );
                }
            }
        }

        UInt32 texture_keys[Material::max_textures];
        Memory::Set(&texture_keys[0], 0, sizeof(texture_keys));

        for (SizeType i = 0, texture_index = 0; i < in_object.GetTextures().Size(); i++) {
            const auto key = in_object.GetTextures().KeyAt(i);
            const auto &value = in_object.GetTextures().ValueAt(i);

            if (value) {
                if (auto err = out.AddChild(*value, true)) {
                    return err;
                }

                texture_keys[texture_index++] = static_cast<UInt32>(key);
            }
        }

        out.SetProperty(
            "texture_keys",
            FBOMArray(FBOMUnsignedInt(), std::size(texture_keys)),
            &texture_keys[0]
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize( const FBOMObject &in, UniquePtr<Material> &out_object) const override
    {
        String name;
        in.GetProperty("name").ReadString(name);

        out_object.Reset(new Material(name));

        in.GetProperty("attributes.bucket").ReadUnsignedInt(&out_object->GetRenderAttributes().bucket);
        in.GetProperty("attributes.flags").ReadUnsignedInt(&out_object->GetRenderAttributes().flags);
        in.GetProperty("attributes.cull_mode").ReadUnsignedInt(&out_object->GetRenderAttributes().cull_faces);
        in.GetProperty("attributes.fill_mode").ReadUnsignedInt(&out_object->GetRenderAttributes().fill_mode);

        UInt num_parameters;

        // load parameters
        if (auto err = in.GetProperty("params.size").ReadUnsignedInt(&num_parameters)) {
            return err;
        }

        for (UInt i = 0; i < num_parameters; i++) {
            const auto param_string = String("params.") + String::ToString(i);

            Material::MaterialKey key;
            Material::Parameter param;

            if (auto err = in.GetProperty(param_string).ReadArrayElements(FBOMFloat(), 4, &param.values)) {
                continue;
            }

            if (auto err = in.GetProperty(param_string + ".key").ReadUnsignedLong(&key)) {
                continue;
            }

            if (auto err = in.GetProperty(param_string + ".type").ReadUnsignedInt(&param.type)) {
                continue;
            }

            if (param.IsIntType()) {
                for (UInt j = 0; j < 4; j++) {
                    in.GetProperty(param_string + ".values[" + String::ToString(j) + "]")
                        .ReadInt(&param.values.int_values[j]);
                }
            } else if (param.IsFloatType()) {
                for (UInt j = 0; j < 4; j++) {
                    in.GetProperty(param_string + ".values[" + String::ToString(j) + "]")
                        .ReadFloat(&param.values.float_values[j]);
                }
            }

            out_object->SetParameter(key, param);
        }

        UInt32 texture_keys[Material::max_textures];
        Memory::Set(&texture_keys[0], 0, sizeof(texture_keys));

        if (auto err = in.GetProperty("texture_keys").ReadArrayElements(FBOMUnsignedInt(), std::size(texture_keys), &texture_keys[0])) {
            return err;
        }

        UInt texture_index = 0;

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("Texture")) {
                if (texture_index < std::size(texture_keys)) {
                    if (auto texture = node.deserialized.Get<Texture>()) {
                        out_object->SetTexture(
                            static_cast<Material::TextureKey>(texture_keys[texture_index]), 
                            std::move(texture)
                        );

                        ++texture_index;
                    }
                }
            }
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif