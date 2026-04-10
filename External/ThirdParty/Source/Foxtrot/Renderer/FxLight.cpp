#include "FxLight.hpp"

#include "FxCamera.hpp"
#include "FxEngine.hpp"

#include <FxObjectManager.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


using VertexType = FxLightBase::VertexType;

FxLightBase::FxLightBase(FxLightFlags flags) : Flags(flags)
{
    ObjectId = gObjectManager->GenerateObjectId();

    FxLogDebug("Creating light (id={})", ObjectId);
}

void FxLightBase::SetLightVolume(const FxRef<FxPrimitiveMesh>& volume) { pLightVolume = volume; }

void FxLightBase::SetLightVolume(const FxRef<FxMeshGen::GeneratedMesh>& volume_gen, bool create_debug_mesh)
{
    pLightVolumeGen = volume_gen;
    pLightVolume = volume_gen->AsSlimMesh();
    // Radius = LightVolume->VertexList.CalculateDimensionsFromPositions().X;

    if (create_debug_mesh) {
        mpDebugMesh = volume_gen->AsDefaultMesh();
    }
}


// void FxLightBase::MoveTo(const FxVec3f& position)
// {
//     if ((Flags & FxLF_IndependentMeshPosition)) {
//         mLightPosition = position;

//         // The model matrix does not need to be updated as we won't move the mesh
//         return;
//     }

//     this->FxEntity::MoveTo(position);
//     mLightPosition = mPosition;
// }


// void FxLightBase::MoveTo(const FxVec3f& position, bool force_move_mesh)
// {
//     this->MoveTo(position);

//     if (force_move_mesh && !(Flags & FxLF_IndependentMeshPosition)) {
//         this->FxEntity::MoveTo(position);
//         mLightPosition = mPosition;
//     }
// }


// void FxLightBase::MoveBy(const FxVec3f& offset)
// {
//     if ((Flags & FxLF_IndependentMeshPosition)) {
//         mLightPosition += offset;

//         // The model matrix does not need to be updated as we won't move the mesh
//         return;
//     }

//     this->FxEntity::MoveBy(offset);
//     mLightPosition = mPosition;
// }


// void FxLightBase::MoveBy(const FxVec3f& offset, bool force_move_mesh)
// {
//     this->MoveBy(offset);

//     if (force_move_mesh && !(Flags & FxLF_IndependentMeshPosition)) {
//         this->FxEntity::MoveBy(offset);
//         mLightPosition = mPosition;
//     }
// }

// void FxLightBase::Scale(const FxVec3f& scale)
// {
//     this->FxEntity::Scale(scale);
//     Radius *= scale.X;
// }

void FxLightBase::Render(const FxPerspectiveCamera& camera, FxCamera* shadow_camera)
{
    if (!bEnabled) {
        return;
    }

    if (camera.Position.IntersectsSphere(mPosition, mRadius)) {
        pPipeline = pPipelineInside;
    }
    else {
        pPipeline = pPipelineOutside;
    }

    RxFrameData* frame = gRenderer->GetFrame();
    UpdateIfOutOfDate();

    gRenderer->Uniforms.Rewind();

    pPipeline->Bind(frame->CommandBuffer);

    {
        FxLightVertPushConstants push_constants {};
        memcpy(push_constants.CameraMatrix, camera.GetCameraMatrix(FxObjectLayer::eWorldLayer).RawData,
               sizeof(FxMat4f));


        push_constants.ObjectId = ObjectId;

        vkCmdPushConstants(frame->CommandBuffer.Get(), pPipeline->Layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(push_constants), &push_constants);
    }


    gRenderer->Uniforms.WritePtr(shadow_camera->GetCameraMatrix(FxObjectLayer::eWorldLayer).RawData, sizeof(FxMat4f));
    gRenderer->Uniforms.WritePtr(camera.InvViewMatrix.RawData, sizeof(FxMat4f));
    gRenderer->Uniforms.WritePtr(camera.InvProjectionMatrix.RawData, sizeof(FxMat4f));

    // Note that the light position is packed with the light colour as the fourth component!
    gRenderer->Uniforms.WritePtr(mPosition.mData, sizeof(float32) * 3);
    gRenderer->Uniforms.Write(Color.Value);

    gRenderer->Uniforms.WritePtr(camera.Position.mData, sizeof(float32) * 3);
    gRenderer->Uniforms.Write(mRadius);

    gRenderer->Uniforms.Write(static_cast<float32>(gRenderer->Swapchain.Extent.X));
    gRenderer->Uniforms.Write(static_cast<float32>(gRenderer->Swapchain.Extent.Y));

    gRenderer->Uniforms.Write(AmbientColor.Value);

    gRenderer->Uniforms.FlushToGpu();

    pLightVolume->Render(frame->CommandBuffer, 1);
}


void FxLightBase::RenderDebugMesh(const FxPerspectiveCamera& camera)
{
    if (!mpDebugMesh) {
        return;
    }

    RxFrameData* frame = gRenderer->GetFrame();
    FxRef<RxDeferredRenderer>& deferred = gRenderer->pDeferredRenderer;

    FxDrawPushConstants push_constants {};
    memcpy(push_constants.CameraMatrix, camera.GetCameraMatrix(FxObjectLayer::eWorldLayer).RawData, sizeof(FxMat4f));
    push_constants.ObjectId = ObjectId;

    vkCmdPushConstants(frame->CommandBuffer.CommandBuffer, deferred->PlGeometry.Layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),
                       &push_constants);

    mpDebugMesh->Render(frame->CommandBuffer, 1);
}


void FxLightBase::SetRadius(const float radius)
{
    mRadius = radius;
    SetScale(mRadius * 2);
}

FxLightPoint::FxLightPoint()
{
    pPipelineInside = &gRenderer->pDeferredRenderer->PlLightingInsideVolume;
    pPipelineOutside = &gRenderer->pDeferredRenderer->PlLightingOutsideVolume;
}


FxLightDirectional::FxLightDirectional() { pPipeline = &gRenderer->pDeferredRenderer->PlLightingDirectional; }

void FxLightDirectional::Render(const FxPerspectiveCamera& camera, FxCamera* shadow_camera)
{
    if (!bEnabled) {
        return;
    }

    RxFrameData* frame = gRenderer->GetFrame();
    UpdateIfOutOfDate();

    pPipeline->Bind(frame->CommandBuffer);

    {
        FxLightVertPushConstants push_constants {};
        memcpy(push_constants.CameraMatrix, camera.GetCameraMatrix(FxObjectLayer::eWorldLayer).RawData,
               sizeof(FxMat4f));

        push_constants.ObjectId = ObjectId;

        vkCmdPushConstants(frame->CommandBuffer.Get(), pPipeline->Layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(push_constants), &push_constants);
    }

    gRenderer->Uniforms.Rewind();

    if (shadow_camera) {
        gRenderer->Uniforms.WritePtr(shadow_camera->GetCameraMatrix(FxObjectLayer::eWorldLayer).RawData,
                                     sizeof(FxMat4f));
    }
    else {
        gRenderer->Uniforms.WritePtr(camera.GetCameraMatrix(FxObjectLayer::eWorldLayer).RawData, sizeof(FxMat4f));
    }

    gRenderer->Uniforms.WritePtr(camera.InvViewMatrix.RawData, sizeof(FxMat4f));
    gRenderer->Uniforms.WritePtr(camera.InvProjectionMatrix.RawData, sizeof(FxMat4f));

    // // Note that the light position is packed with the light colour as the fourth component!
    gRenderer->Uniforms.WritePtr(camera.Position.mData, sizeof(float32) * 3);
    gRenderer->Uniforms.Write(mRadius);

    gRenderer->Uniforms.WritePtr(mPosition.mData, sizeof(float32) * 3);
    gRenderer->Uniforms.Write(Color.Value);

    gRenderer->Uniforms.Write(static_cast<float32>(gRenderer->Swapchain.Extent.X));
    gRenderer->Uniforms.Write(static_cast<float32>(gRenderer->Swapchain.Extent.Y));

    gRenderer->Uniforms.Write(AmbientColor.Value);

    gRenderer->Uniforms.FlushToGpu();

    // gRenderer->Uniforms.AssertSize(sizeof(FxLightFragPushConstants));

    vkCmdDraw(frame->CommandBuffer.Get(), 3, 1, 0, 0);
}
