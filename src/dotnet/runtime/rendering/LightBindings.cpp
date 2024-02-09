#include <dotnet/runtime/ManagedHandle.hpp>
#include <dotnet/runtime/math/ManagedMathTypes.hpp>

#include <rendering/Light.hpp>

#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <type_traits>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    uint32 Light_GetTypeID()
    {
        return TypeID::ForType<Light>().Value();
    }

    ManagedHandle Light_Create(
        uint32 type,
        ManagedVec3f position,
        uint32 color,
        float intensity,
        float radius
    )
    {
        return CreateManagedHandleFromHandle(CreateObject<Light>(
            LightType(type),
            position,
            Color(color),
            intensity,
            radius
        ));
    }

    void Light_Init(ManagedHandle light_handle)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return;
        }

        InitObject(light);
    }

    ManagedVec3f Light_GetPosition(ManagedHandle light_handle)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return { };
        }

        return light->GetPosition();
    }

    void Light_SetPosition(ManagedHandle light_handle, ManagedVec3f position)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return;
        }

        light->SetPosition(position);
    }

    uint32 Light_GetColor(ManagedHandle light_handle)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return 0;
        }

        return uint32(light->GetColor());
    }

    void Light_SetColor(ManagedHandle light_handle, uint32 color)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return;
        }

        light->SetColor(Color(color));
    }

    float Light_GetIntensity(ManagedHandle light_handle)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return 0.0f;
        }

        return light->GetIntensity();
    }

    void Light_SetIntensity(ManagedHandle light_handle, float intensity)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return;
        }

        light->SetIntensity(intensity);
    }

    float Light_GetRadius(ManagedHandle light_handle)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return 0.0f;
        }

        return light->GetRadius();
    }

    void Light_SetRadius(ManagedHandle light_handle, float radius)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return;
        }

        light->SetRadius(radius);
    }

    float Light_GetFalloff(ManagedHandle light_handle)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return 0.0f;
        }

        return light->GetFalloff();
    }

    void Light_SetFalloff(ManagedHandle light_handle, float falloff)
    {
        Handle<Light> light = CreateHandleFromManagedHandle<Light>(light_handle);

        if (!light) {
            return;
        }

        light->SetFalloff(falloff);
    }
}