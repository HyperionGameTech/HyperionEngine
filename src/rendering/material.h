#ifndef MATERIAL_H
#define MATERIAL_H

#include "../hash_code.h"
#include "texture.h"
#include "../math/vector2.h"
#include "../math/vector3.h"
#include "../math/vector4.h"
#include "../asset/fbom/fbom.h"
#include "../util/enum_options.h"

#include <string>
#include <array>
#include <map>
#include <memory>

#define MATERIAL_MAX_PARAMETERS 32

namespace hyperion {

enum MaterialParameterKey {
    MATERIAL_PARAMETER_NONE = 0b00,
    
    // basic
    MATERIAL_PARAMETER_DIFFUSE_COLOR = 0b10,
    MATERIAL_PARAMETER_METALNESS     = 0b100,
    MATERIAL_PARAMETER_ROUGHNESS     = 0b1000,
    MATERIAL_PARAMETER_EMISSIVENESS  = 0b10000,
    MATERIAL_PARAMETER_FLIP_UV       = 0b100000,
    MATERIAL_PARAMETER_RESERVED_1    = 0b1000000,
    MATERIAL_PARAMETER_RESERVED_2    = 0b10000000,
    MATERIAL_PARAMETER_RESERVED_3    = 0b100000000,
    MATERIAL_PARAMETER_RESERVED_4    = 0b1000000000,
    MATERIAL_PARAMETER_RESERVED_5    = 0b10000000000,
    MATERIAL_PARAMETER_RESERVED_6    = 0b100000000000,
    MATERIAL_PARAMETER_RESERVED_7    = 0b1000000000000,
    MATERIAL_PARAMETER_RESERVED_8    = 0b10000000000000,
    MATERIAL_PARAMETER_RESERVED_9    = 0b100000000000000,

    // terrain
    MATERIAL_PARAMETER_TERRAIN_LEVEL_0_HEIGHT = 0b1000000000000000,
    MATERIAL_PARAMETER_TERRAIN_LEVEL_1_HEIGHT = 0b10000000000000000,
    MATERIAL_PARAMETER_TERRAIN_LEVEL_2_HEIGHT = 0b100000000000000000,
    MATERIAL_PARAMETER_TERRAIN_LEVEL_3_HEIGHT = 0b1000000000000000000
};

enum MaterialParameterType {
    MaterialParameter_None,
    MaterialParameter_Float,
    MaterialParameter_Int,
    MaterialParameter_Texture,
    MaterialParameter_Vector2,
    MaterialParameter_Vector3,
    MaterialParameter_Vector4
};

enum MaterialFaceCull {
    MaterialFace_None = 0x00,
    MaterialFace_Front = 0x01,
    MaterialFace_Back = 0x02
};

class MaterialParameter {
public:
    MaterialParameter();
    MaterialParameter(const float value);
    MaterialParameter(const float *data, size_t nvalues, MaterialParameterType paramtype);
    MaterialParameter(const MaterialParameter &other);
    ~MaterialParameter() = default;

    inline MaterialParameterType GetType() const { return MaterialParameterType(type); }

    inline float &operator[](size_t idx) { return values[idx]; }
    inline float operator[](size_t idx) const { return values[idx]; }
    inline bool operator==(const MaterialParameter &other) const
    {
        return values == other.values
            && type == other.type;
    }
    inline bool operator!=(const MaterialParameter &other) const { return !operator==(other); }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(int(type));

        for (int i = 0; i < values.size(); i++) {
            hc.Add(values[i]);
        }

        return hc;
    }

private:
    int32_t type;
    std::array<float, 4> values;
};

class Material : public fbom::FBOMLoadable {
public:
    using MaterialParameterTable_t = EnumOptions<MaterialParameterKey, MaterialParameter, MATERIAL_MAX_PARAMETERS>;

    static const EnumOptions<MaterialParameterKey, MaterialParameter, 3> default_parameters;

    Material();
    Material(const Material &other);
    virtual ~Material() = default;

    inline MaterialParameterTable_t &GetParameters() { return m_params; }
    inline const MaterialParameter &GetParameter(MaterialParameterKey key) const { return m_params.Get(key); }

    void SetParameter(MaterialParameterKey, float);
    void SetParameter(MaterialParameterKey, int);
    void SetParameter(MaterialParameterKey, const Vector2 &);
    void SetParameter(MaterialParameterKey, const Vector3 &);
    void SetParameter(MaterialParameterKey, const Vector4 &);

    void SetTexture(const std::string &name, const std::shared_ptr<Texture> &);
    std::shared_ptr<Texture> GetTexture(const std::string &name) const;

    MaterialFaceCull cull_faces = MaterialFace_Back;

    bool alpha_blended = false;
    bool depth_test = true;
    bool depth_write = true;
    Vector4 diffuse_color = Vector4(1.0);

    std::map<std::string, std::shared_ptr<Texture>> textures;

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_params.GetHashCode());

        for (const auto &it : textures) {
            if (it.second == nullptr) {
                continue;
            }

            hc.Add(it.first);
            hc.Add(intptr_t(it.second->GetBytes())); // pointer to memory of image
        }

        hc.Add(alpha_blended);
        hc.Add(depth_test);
        hc.Add(depth_write);
        hc.Add(diffuse_color.GetHashCode());
        hc.Add(int(cull_faces));

        return hc;
    }

    virtual std::shared_ptr<Loadable> Clone() override;


#pragma region serialization
    FBOM_DEF_DESERIALIZER(loader, in, out) {
        using namespace fbom;

        static_assert(sizeof(MaterialParameter) == 4 + 16); /* sizeof(int32_t) + sizeof(float) * 4 */
        static_assert(sizeof(MaterialParameterTable_t) == (sizeof(MaterialParameter) * MATERIAL_MAX_PARAMETERS) + 8); /* uint64_t */

        auto out_material = std::make_shared<Material>();
        out = out_material;

        if (auto err = in->GetProperty("diffuse_color").ReadArrayElements(FBOMFloat(), 4, (unsigned char *)&out_material->diffuse_color)) {
            return err;
        }

        if (auto err = in->GetProperty("parameters").ReadStruct(sizeof(MaterialParameterTable_t), (unsigned char *)&out_material->m_params)) {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }

    FBOM_DEF_SERIALIZER(loader, in, out)
    {
        using namespace fbom;

        auto material = dynamic_cast<Material*>(in);

        if (material == nullptr) {
            return FBOMResult::FBOM_ERR;
        }
 
        out->SetProperty("diffuse_color", FBOMArray(FBOMFloat(), 4), (void*)&material->diffuse_color);
        out->SetProperty("parameters", FBOMStruct(sizeof(MaterialParameterTable_t)), (void*)&material->m_params);

        return FBOMResult::FBOM_OK;
    }
#pragma endregion serialization

private:
    MaterialParameterTable_t m_params;

    std::shared_ptr<Material> CloneImpl();
};

} // namespace hyperion

#endif
