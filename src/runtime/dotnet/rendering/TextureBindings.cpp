#include <runtime/dotnet/ManagedHandle.hpp>

#include <rendering/Texture.hpp>

#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <type_traits>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    UInt32 Texture_GetTypeID()
    {
        return TypeID::ForType<Texture>().Value();
    }

    ManagedHandle Texture_Create()
    {
        return CreateManagedHandleFromHandle(CreateObject<Texture>());
    }

    void Texture_Init(ManagedHandle texture_handle)
    {
        Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

        if (!texture) {
            return;
        }

        InitObject(texture);
    }

    UInt32 Texture_GetInternalFormat(ManagedHandle texture_handle)
    {
        Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

        if (!texture) {
            return 0;
        }

        return UInt32(texture->GetFormat());
    }

    UInt32 Texture_GetFilterMode(ManagedHandle texture_handle)
    {
        Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

        if (!texture) {
            return 0;
        }

        return UInt32(texture->GetFilterMode());
    }

    UInt32 Texture_GetImageType(ManagedHandle texture_handle)
    {
        Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

        if (!texture) {
            return 0;
        }

        return UInt32(texture->GetType());
    }

    ManagedHandle Material_GetTexture(ManagedHandle material_handle, UInt64 texture_key)
    {
        Handle<Material> material = CreateHandleFromManagedHandle<Material>(material_handle);

        if (!material) {
            return { };
        }

        return CreateManagedHandleFromHandle<Texture>(material->GetTexture(Material::TextureKey(texture_key)));
    }

    void Material_SetTexture(ManagedHandle material_handle, UInt64 texture_key, ManagedHandle texture_handle)
    {
        Handle<Material> material = CreateHandleFromManagedHandle<Material>(material_handle);

        if (!material) {
            return;
        }

        Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

        material->SetTexture(Material::TextureKey(texture_key), std::move(texture));
    }
}