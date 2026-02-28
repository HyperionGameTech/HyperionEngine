/// TODO: refactor to use mappings instead of idx (void* directly to element on cpu)
typedef void (*WriteBufferDataFunction)(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

template <class T>
static void OnBindingChanged_Default(T* resource, uint32 prev, uint32 next)
{
#if HYP_DEBUG_MODE
    AssertOnThread(g_renderThread);
#endif

    AssertDebug(resource != nullptr);

    AssignResourceBinding(resource, next);
}

template <class ProxyType>
static void WriteBufferData_Default(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy)
{
    AssertDebug(gpuBufferHolder != nullptr);
    AssertDebug(idx != ~0u, "Invalid index for writing buffer data!");

    ProxyType* proxyCasted = static_cast<ProxyType*>(proxy);
    AssertDebug(proxyCasted != nullptr, "Proxy is null!");

    gpuBufferHolder->WriteBufferData(idx, &proxyCasted->bufferData, sizeof(proxyCasted->bufferData));
}

extern void OnBindingChanged_MeshEntity(Entity* envProbe, uint32 prev, uint32 next);
extern void WriteBufferData_MeshEntity(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Mesh(Mesh* mesh, uint32 prev, uint32 next);

// for setting texture only
extern void OnBindingChanged_ReflectionProbe(EnvProbe* envProbe, uint32 prev, uint32 next);

extern void OnBindingChanged_EnvProbe(EnvProbe* envProbe, uint32 prev, uint32 next);
extern void WriteBufferData_EnvProbe(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_EnvGrid(EnvGrid* envGrid, uint32 prev, uint32 next);

extern void OnBindingChanged_Light(Light* light, uint32 prev, uint32 next);
extern void WriteBufferData_Light(GpuBufferHolderBase* gpuBufferHolder, uint32 idx, IRenderProxy* proxy);

extern void OnBindingChanged_Material(Material* lightmapVolume, uint32 prev, uint32 next);

extern void OnBindingChanged_Texture(Texture* texture, uint32 prev, uint32 next);

static ResourceBindingAllocator<> s_meshEntityBindingsAllocator;
static ResourceBinder<Entity, &OnBindingChanged_MeshEntity> s_meshEntityBinder { &s_meshEntityBindingsAllocator };
ResourceBinderBase* g_meshEntityBinder = &s_meshEntityBinder;

static ResourceBindingAllocator<> s_meshBindingsAllocator;
static ResourceBinder<Mesh, &OnBindingChanged_Mesh> s_meshBinder { &s_meshBindingsAllocator };
ResourceBinderBase* g_meshBinder = &s_meshBinder;

static ResourceBindingAllocator<> s_cameraBindingsAllocator;
static ResourceBinder<Camera, &OnBindingChanged_Default<Camera>> s_cameraBinder { &s_cameraBindingsAllocator };
ResourceBinderBase* g_cameraBinder = &s_cameraBinder;

// Shared index allocator for all envprobes

static ResourceBindingAllocator<> s_envProbeBindingsAllocator;
static ResourceBinder<EnvProbe, &OnBindingChanged_EnvProbe> s_envProbeBinder { &s_envProbeBindingsAllocator };
ResourceBinderBase* g_envProbeBinder = &s_envProbeBinder;

// reflection / sky probes need to allocate texture slots
static ResourceBindingAllocator<MaxBoundReflectionProbes> s_reflectionProbeTextureBindingsAllocator;
static ResourceBinder<EnvProbe, &OnBindingChanged_ReflectionProbe> s_reflectionProbeTextureBinder { &s_reflectionProbeTextureBindingsAllocator };
ResourceBinderBase* g_reflectionProbeTextureBinder = &s_reflectionProbeTextureBinder;

static ResourceBindingAllocator<MaxBoundEnvGrids> s_envGridBindingsAllocator;
static ResourceBinder<EnvGrid, &OnBindingChanged_EnvGrid> s_envGridBinder { &s_envGridBindingsAllocator };
ResourceBinderBase* g_envGridBinder = &s_envGridBinder;

static ResourceBindingAllocator<> s_lightBindingsAllocator;
static ResourceBinder<Light, &OnBindingChanged_Light> s_lightBinder { &s_lightBindingsAllocator };
ResourceBinderBase* g_lightBinder = &s_lightBinder;

static ResourceBindingAllocator<MaxBoundLightmapVolumes> s_lightmapVolumeBindingsAllocator;
static ResourceBinder<LightmapVolume, &OnBindingChanged_Default<LightmapVolume>> s_lightmapVolumeBinder {
    &s_lightmapVolumeBindingsAllocator
};
ResourceBinderBase* g_lightmapVolumeBinder = &s_lightmapVolumeBinder;

static ResourceBindingAllocator<> s_particleVolumeBindingsAllocator;
static ResourceBinder<ParticleVolume, &OnBindingChanged_Default<ParticleVolume>> s_particleVolumeBinder {
    &s_particleVolumeBindingsAllocator
};
ResourceBinderBase* g_particleVolumeBinder = &s_particleVolumeBinder;

static ResourceBindingAllocator<> s_fogVolumeBindingsAllocator;
static ResourceBinder<FogVolume, &OnBindingChanged_Default<FogVolume>> s_fogVolumeBinder {
    &s_fogVolumeBindingsAllocator
};
ResourceBinderBase* g_fogVolumeBinder = &s_fogVolumeBinder;

static ResourceBindingAllocator<> s_materialBindingsAllocator;
static ResourceBinder<Material, &OnBindingChanged_Material> s_materialBinder { &s_materialBindingsAllocator };
ResourceBinderBase* g_materialBinder = &s_materialBinder;

static ResourceBindingAllocator<> s_textureBindingsAllocator;
static ResourceBinder<Texture, &OnBindingChanged_Texture> s_textureBinder { &s_textureBindingsAllocator };
ResourceBinderBase* g_textureBinder = &s_textureBinder;

static ResourceBindingAllocator<> s_skeletonBindingsAllocator;
static ResourceBinder<Skeleton, &OnBindingChanged_Default<Skeleton>> s_skeletonBinder { &s_skeletonBindingsAllocator };
ResourceBinderBase* g_skeletonBinder = &s_skeletonBinder;

static ResourceBinderBase* s_resourceBinders[] = {
    &s_meshEntityBinder,
    &s_meshBinder,
    &s_cameraBinder,
    &s_envProbeBinder,
    &s_reflectionProbeTextureBinder,
    &s_envGridBinder,
    &s_lightBinder,
    &s_lightmapVolumeBinder,
    &s_particleVolumeBinder,
    &s_fogVolumeBinder,
    &s_materialBinder,
    &s_textureBinder,
    &s_skeletonBinder
};