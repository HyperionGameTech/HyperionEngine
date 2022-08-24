#ifndef HYPERION_V2_FBOM_MARSHALS_MATERIAL_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_MATERIAL_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
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
        return FBOMObjectType("Material");
    }

    virtual FBOMResult Serialize(const Material &in_object, FBOMObject &out) const override
    {
        for (SizeType i = 0; i < in_object.GetParameters().Size(); i++) {
            const auto &key_value = in_object.GetParameters().KeyValueAt(i);

            out.SetProperty(
                String::ToString(static_cast<UInt>(key_value.first)),
                FBOMStruct(sizeof(Material::Parameter)),
                key_value.second
            );
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(Engine *, const FBOMObject &in, Material *&out_object) const override
    {
        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif