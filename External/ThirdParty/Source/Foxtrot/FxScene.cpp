#include "FxScene.hpp"

#include <FxEngine.hpp>
#include <FxObjectManager.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>
#include <Renderer/RxShadowDirectional.hpp>

void FxScene::Create()
{
    mObjects.Create(32);
    mLights.Create(32);
}

void FxScene::Attach(const FxTSRef<FxObject>& object)
{
    mObjects.Insert(object);
    object->OnAttached(this);
}
void FxScene::Attach(const FxRef<FxLightBase>& light)
{
    mLights.Insert(light);
    light->OnAttached(this);
}

FxTSRef<FxObject> FxScene::FindObject(FxHash64 name_hash)
{
    for (FxTSRef<FxObject>& obj : mObjects) {
        if (obj->Name == name_hash) {
            return obj;
        }
    }

    return FxTSRef<FxObject>(nullptr);
}


void FxScene::Render(FxCamera* shadow_camera)
{
    FxPerspectiveCamera& camera = *mpCurrentCamera;

    gRenderer->BeginGeometry();

    for (const FxTSRef<FxObject>& obj : mObjects) {
        obj->Update();


        if (obj->GetRenderUnlit()) {
            continue;
        }

        obj->Render(camera);
    }

    // Render lights
    gRenderer->BeginLighting();


    for (const FxRef<FxLightBase>& light : mLights) {
        light->Render(camera, shadow_camera);
    }

    RenderUnlitObjects(camera);

    gRenderer->DoComposition(camera);
}

void FxScene::RenderUnlitObjects(const FxCamera& camera) const
{
    // gRenderer->pDeferredRenderer->PlUnlit.Bind(gRenderer->GetFrame()->CommandBuffer);
    gRenderer->BeginUnlit();


    for (const FxTSRef<FxObject>& obj : mObjects) {
        if (!obj->GetRenderUnlit()) {
            continue;
        }
        obj->RenderUnlit(camera);
    }
}

void FxScene::RenderShadows(FxCamera* shadow_camera)
{
    gShadowRenderer->Begin();

    RxShadowPushConstants consts;

    memcpy(consts.CameraMatrix, gShadowRenderer->ShadowCamera.GetCameraMatrix(FxObjectLayer::eWorldLayer).RawData,
           sizeof(float32) * 16);

    bool in_skinned_shader = false;

    RxPipeline* pipeline = &gShadowRenderer->GetPipeline();

    RxCommandBuffer& cmd = gRenderer->GetFrame()->CommandBuffer;

    for (const FxTSRef<FxObject>& obj : mObjects) {
        if (!obj->IsShadowCaster()) {
            continue;
        }


        if (in_skinned_shader && !obj->IsSkinned()) {
            pipeline = &gShadowRenderer->GetPipeline();
            in_skinned_shader = false;
            pipeline->Bind(cmd);

            gObjectManager->mObjectBufferDS.BindWithOffset(0, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline,
                                                           gObjectManager->GetBaseOffset());
        }
        if (obj->IsSkinned()) {
            pipeline = &gShadowRenderer->GetSkinnedPipeline();
            in_skinned_shader = true;
            pipeline->Bind(cmd);

            gObjectManager->mObjectBufferDS.BindWithOffset(0, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline,
                                                           gObjectManager->GetBaseOffset());
        }

        obj->Update();

        consts.ObjectId = obj->ObjectId;

        vkCmdPushConstants(cmd.Get(), pipeline->Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RxShadowPushConstants),
                           &consts);

        obj->RenderPrimitive(cmd);
    }

    gShadowRenderer->End();
}

void FxScene::Destroy()
{
    mObjects.Destroy();
    mLights.Destroy();
}
