#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <rendering/Material.hpp>

#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <type_traits>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    struct ManagedMaterialParameter
    {
        float   value[4];
        uint32  type;

        ManagedMaterialParameter() = default;

        ManagedMaterialParameter(const Material::Parameter &param)
            : type(uint32(param.type))
        {
            // hacky way to copy the data
            param.Copy(reinterpret_cast<uint8 *>(value));
        }

        operator Material::Parameter() const
        {
            Material::Parameter param;
            param.type = Material::Parameter::Type(type);

            // hacky way to copy the data
            Memory::MemCpy(&param.values, value, sizeof(value));

            return param;
        }
    };

    static_assert(std::is_trivial_v<ManagedMaterialParameter>, "ManagedMaterialParameter must be a trivial type to be used in C#");
    static_assert(sizeof(ManagedMaterialParameter) == 20, "ManagedMaterialParameter must be 20 bytes to be used in C#");
    static_assert(sizeof(ManagedMaterialParameter) == sizeof(Material::Parameter), "ManagedMaterialParameter must be the same size as Material::Parameter to maintain consistency");

    uint32 Material_GetTypeID()
    {
        return TypeID::ForType<Material>().Value();
    }

    ManagedHandle Material_Create()
    {
        return CreateManagedHandleFromHandle(CreateObject<Material>());
    }

    void Material_Init(ManagedHandle material_handle)
    {
        Handle<Material> material = CreateHandleFromManagedHandle<Material>(material_handle);

        if (!material) {
            return;
        }

        InitObject(material);
    }

    ManagedMaterialParameter Material_GetParameter(ManagedHandle material_handle, uint64 key)
    {
        Handle<Material> material = CreateHandleFromManagedHandle<Material>(material_handle);

        if (!material) {
            return { };
        }

        return ManagedMaterialParameter(material->GetParameter(Material::MaterialKey(key)));
    }

    void Material_SetParameter(ManagedHandle material_handle, uint64 key, ManagedMaterialParameter param)
    {
        Handle<Material> material = CreateHandleFromManagedHandle<Material>(material_handle);

        if (!material) {
            return;
        }

        material->SetParameter(Material::MaterialKey(key), Material::Parameter(param));
    }
}