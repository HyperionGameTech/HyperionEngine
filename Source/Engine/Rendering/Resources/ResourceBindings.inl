typedef void (*WriteBufferDataFunction)(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy);

template <class ProxyType>
static void WriteBufferData_Default(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(idx != ~0u, "Invalid index for writing buffer data!");

    ProxyType* proxyCasted = static_cast<ProxyType*>(proxy);
    AssertDebug(proxyCasted != nullptr, "Proxy is null!");

    sbuffer.Write(idx * sizeof(proxyCasted->bufferData), sizeof(proxyCasted->bufferData), &proxyCasted->bufferData);
}

extern void WriteBufferData_MeshEntity(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Mesh(Mesh* mesh, uint32 prev, uint32 next);

extern void OnBindingChanged_EnvProbe(EnvProbe* envProbe, uint32 prev, uint32 next);
extern void WriteBufferData_EnvProbe(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy);

// for setting texture only
extern void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next);

extern void WriteBufferData_Light(StructuredBuffer& sbuffer, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Material(MaterialInstance* material, uint32 prev, uint32 next);

extern void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next);

static ResourceBindingAllocator<MaxBoundEntities> s_meshEntityBindingsAllocator;
static ResourceBinder<Entity> s_meshEntityBinder { &s_meshEntityBindingsAllocator };
ResourceBinderBase* g_meshEntityBinder = &s_meshEntityBinder;

static ResourceBindingAllocator<MaxBoundMeshes> s_meshBindingsAllocator;
static ResourceBinder<Mesh, &OnBindingChanged_Mesh> s_meshBinder { &s_meshBindingsAllocator };
ResourceBinderBase* g_meshBinder = &s_meshBinder;

static ResourceBindingAllocator<MaxBoundCameras> s_cameraBindingsAllocator;
static ResourceBinder<Camera> s_cameraBinder { &s_cameraBindingsAllocator };
ResourceBinderBase* g_cameraBinder = &s_cameraBinder;

// Shared index allocator for all envprobes

static ResourceBindingAllocator<MaxBoundEnvProbes> s_envProbeBindingsAllocator;
static ResourceBinder<EnvProbe, &OnBindingChanged_EnvProbe> s_envProbeBinder { &s_envProbeBindingsAllocator };
ResourceBinderBase* g_envProbeBinder = &s_envProbeBinder;

// reflection / sky probes need to allocate texture slots
static ResourceBindingAllocator<MaxBoundReflectionProbes> s_reflectionProbeTextureBindingsAllocator;
static ResourceBinder<EnvProbe, &OnBindingChanged_ReflectionProbe> s_reflectionProbeTextureBinder { &s_reflectionProbeTextureBindingsAllocator };
ResourceBinderBase* g_reflectionProbeTextureBinder = &s_reflectionProbeTextureBinder;

static ResourceBindingAllocator<MaxBoundProbeVolumes> s_probeVolumeBindingsAllocator;
static ResourceBinder<ProbeVolume> s_probeVolumeBinder { &s_probeVolumeBindingsAllocator };
ResourceBinderBase* g_probeVolumeBinder = &s_probeVolumeBinder;

static ResourceBindingAllocator<MaxBoundLights> s_lightBindingsAllocator;
static ResourceBinder<Light> s_lightBinder { &s_lightBindingsAllocator };
ResourceBinderBase* g_lightBinder = &s_lightBinder;

static ResourceBindingAllocator<MaxBoundLightmapVolumes> s_lightmapVolumeBindingsAllocator;
static ResourceBinder<LightmapVolume> s_lightmapVolumeBinder {
    &s_lightmapVolumeBindingsAllocator
};
ResourceBinderBase* g_lightmapVolumeBinder = &s_lightmapVolumeBinder;

static ResourceBindingAllocator<MaxBoundParticleVolumes> s_particleVolumeBindingsAllocator;
static ResourceBinder<ParticleVolume> s_particleVolumeBinder {
    &s_particleVolumeBindingsAllocator
};
ResourceBinderBase* g_particleVolumeBinder = &s_particleVolumeBinder;

static ResourceBindingAllocator<MaxBoundFogVolumes> s_fogVolumeBindingsAllocator;
static ResourceBinder<FogVolume> s_fogVolumeBinder {
    &s_fogVolumeBindingsAllocator
};
ResourceBinderBase* g_fogVolumeBinder = &s_fogVolumeBinder;

static ResourceBindingAllocator<MaxBoundMaterials> s_materialBindingsAllocator;
static ResourceBinder<MaterialInstance, &OnBindingChanged_Material> s_materialBinder { &s_materialBindingsAllocator };
ResourceBinderBase* g_materialBinder = &s_materialBinder;

static ResourceBindingAllocator<MaxBoundResourceTextures> s_textureBindingsAllocator;
static ResourceBinder<Texture, &OnBindingChanged_Texture> s_textureBinder { &s_textureBindingsAllocator };
ResourceBinderBase* g_textureBinder = &s_textureBinder;

static ResourceBindingAllocator<MaxBoundSkeletons> s_skeletonBindingsAllocator;
static ResourceBinder<Skeleton> s_skeletonBinder { &s_skeletonBindingsAllocator };
ResourceBinderBase* g_skeletonBinder = &s_skeletonBinder;

static ResourceBindingAllocator<MaxBoundSprites> s_spriteBindingsAllocator;
static ResourceBinder<Sprite> s_spriteBinder { &s_spriteBindingsAllocator };
ResourceBinderBase* g_spriteBinder = &s_spriteBinder;

static ResourceBinderBase* s_resourceBinders[] = {
    &s_meshEntityBinder,
    &s_meshBinder,
    &s_cameraBinder,
    &s_probeVolumeBinder,
    &s_lightBinder,
    &s_lightmapVolumeBinder,
    &s_particleVolumeBinder,
    &s_fogVolumeBinder,
    &s_materialBinder,
    &s_textureBinder,
    &s_envProbeBinder,
    &s_reflectionProbeTextureBinder,
    &s_skeletonBinder,
    &s_spriteBinder
};