#pragma once

#include <Asset/FxMeshGen.hpp>
#include <FxColor.hpp>
#include <FxEntity.hpp>
#include <Math/FxMat4.hpp>
#include <Renderer/FxPrimitiveMesh.hpp>

class RxPipeline;
class FxCamera;

enum FxLightFlags : uint16
{
    FxLF_None = 0x0000,
};

enum class FxLightType
{
    eUnknown,
    eDirectional,
    ePoint,
};

FX_DEFINE_ENUM_AS_FLAGS(FxLightFlags);

class FxLightBase : public FxEntity
{
public:
    using VertexType = RxVertex<RxVertexType::eSlim>;

    static constexpr FxEntityType scEntityType = FxEntityType::eLight;

public:
    FxLightBase(FxLightFlags flags = FxLF_None);

    void SetLightVolume(const FxRef<FxPrimitiveMesh>& volume);
    void SetLightVolume(const FxRef<FxMeshGen::GeneratedMesh>& volume_gen, bool create_debug_mesh = false);

    void SetRadius(const float radius);

    virtual void Render(const FxPerspectiveCamera& camera, FxCamera* shadow_camera);
    virtual void RenderDebugMesh(const FxPerspectiveCamera& camera);

    virtual ~FxLightBase() {}

public:
    FxRef<FxPrimitiveMesh> pLightVolume { nullptr };
    FxRef<FxMeshGen::GeneratedMesh> pLightVolumeGen { nullptr };

    RxPipeline* pPipelineInside = nullptr;
    RxPipeline* pPipelineOutside = nullptr;

    FxColor Color = FxColor::sWhite;
    FxColor AmbientColor { 0x101f1f1f };

    FxLightFlags Flags = FxLF_None;

    bool bEnabled = true;

protected:
    FxRef<FxPrimitiveMesh> mpDebugMesh { nullptr };
    RxPipeline* pPipeline = nullptr;

    float32 mRadius = 1.0f;
};


/**
 * @brief
 */
class FxLightPoint : public FxLightBase
{
public:
    static constexpr FxLightType scLightType = FxLightType::ePoint;

public:
    FxLightPoint();
};

/**
 * @brief
 */
class FxLightDirectional : public FxLightBase
{
public:
    static constexpr FxLightType scLightType = FxLightType::eDirectional;

public:
    FxLightDirectional();

    void Render(const FxPerspectiveCamera& camera, FxCamera* shadow_camera) override;
};

////////////////////////////////////
// Entity Validations
////////////////////////////////////

FX_VALIDATE_ENTITY_TYPE(FxLightBase)
