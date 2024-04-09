#include <dotnet/runtime/ManagedHandle.hpp>

#include <rendering/Texture.hpp>

#include <core/lib/TypeID.hpp>
#include <core/lib/Memory.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <type_traits>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
HYP_EXPORT uint32 Texture_GetTypeID()
{
    return TypeID::ForType<Texture>().Value();
}

HYP_EXPORT ManagedHandle Texture_Create()
{
    return CreateManagedHandleFromHandle(CreateObject<Texture>());
}

HYP_EXPORT void Texture_Init(ManagedHandle texture_handle)
{
    Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

    if (!texture) {
        return;
    }

    InitObject(texture);
}

HYP_EXPORT uint32 Texture_GetInternalFormat(ManagedHandle texture_handle)
{
    Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

    if (!texture) {
        return 0;
    }

    return uint32(texture->GetFormat());
}

HYP_EXPORT uint32 Texture_GetFilterMode(ManagedHandle texture_handle)
{
    Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

    if (!texture) {
        return 0;
    }

    return uint32(texture->GetFilterMode());
}

HYP_EXPORT uint32 Texture_GetImageType(ManagedHandle texture_handle)
{
    Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

    if (!texture) {
        return 0;
    }

    return uint32(texture->GetType());
}

HYP_EXPORT ManagedHandle Material_GetTexture(ManagedHandle material_handle, uint64 texture_key)
{
    Handle<Material> material = CreateHandleFromManagedHandle<Material>(material_handle);

    if (!material) {
        return { };
    }

    return CreateManagedHandleFromHandle<Texture>(material->GetTexture(Material::TextureKey(texture_key)));
}

HYP_EXPORT void Material_SetTexture(ManagedHandle material_handle, uint64 texture_key, ManagedHandle texture_handle)
{
    Handle<Material> material = CreateHandleFromManagedHandle<Material>(material_handle);

    if (!material) {
        return;
    }

    Handle<Texture> texture = CreateHandleFromManagedHandle<Texture>(texture_handle);

    material->SetTexture(Material::TextureKey(texture_key), std::move(texture));
}
} // extern "C"