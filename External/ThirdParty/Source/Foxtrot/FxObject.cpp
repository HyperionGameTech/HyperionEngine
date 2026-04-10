#include "FxObject.hpp"

#include <ThirdParty/Jolt/Jolt.h>
#include <ThirdParty/Jolt/Physics/Body/BodyCreationSettings.h>
#include <ThirdParty/Jolt/Physics/Body/MotionType.h>
#include <ThirdParty/Jolt/Physics/Collision/Shape/BoxShape.h>
#include <ThirdParty/Jolt/Physics/EActivation.h>

#include <Core/FxRefUtil.hpp>
#include <FxEngine.hpp>
#include <FxObjectManager.hpp>
#include <FxScene.hpp>
#include <Physics/PhJolt.hpp>
#include <Renderer/FxMeshUtil.hpp>
#include <Renderer/FxPrimitiveMesh.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

FxObject::FxObject() { ObjectId = gObjectManager->GenerateObjectId(); }

void FxObject::Create(const FxRef<FxPrimitiveMesh>& mesh, const FxTSRef<FxMaterial>& material)
{
    pMesh = mesh;
    pMaterial = material;
}

bool FxObject::CheckIfReady()
{
    if (mbReadyToRender) {
        return mbReadyToRender;
    }

    // If this is a container object, check that the attached nodes are ready
    if (!AttachedNodes.IsEmpty()) {
        // If there is a mesh attached to the container as well, check it
        if (pMesh) {
            if (!pMesh->bIsReady) {
                return (mbReadyToRender = false);
            }


            Dimensions = FxMeshUtil::CalculateDimensions(pMesh->GetVertices());
            // Dimensions = pMesh->GetDimensions();
        }

        if (pMaterial && !pMaterial->IsReady()) {
            return (mbReadyToRender = false);
        }

        for (FxTSRef<FxObject>& object : AttachedNodes) {
            if (!object->CheckIfReady()) {
                return (mbReadyToRender = false);
            }
        }

        return (mbReadyToRender = true);
    }


    if (!pMesh) {
        FX_BREAKPOINT;
    }

    // Not a container, ensure there is a material
    if (!pMaterial || !pMaterial->IsReady()) {
        return (mbReadyToRender = false);
    }

    // This is not a container object, just check that the mesh is loaded
    if (!pMesh || !pMesh->bIsReady.load()) {
        return (mbReadyToRender = false);
    }

    // Dimensions = pMesh->GetDimensions();
    Dimensions = FxMeshUtil::CalculateDimensions(pMesh->GetVertices());

    if (pMesh->VertexList.IsSkinned()) {
        pMaterial->pPipeline = &gRenderer->pDeferredRenderer->PlGeometrySkinned;
    }

    return (mbReadyToRender = true);
}


void FxObject::PhysicsCreatePrimitive(PhPrimitiveType primitive_type, const FxVec3f& dimensions,
                                      PhMotionType motion_type, const PhProperties& physics_properties)
{
    OnLoaded(
        [primitive_type, dimensions, motion_type, physics_properties](FxTSRef<AxBase> base_asset)
        {
            FxTSRef<FxObject> asset = base_asset;
            asset->Physics.CreatePrimitiveBody(primitive_type, dimensions, motion_type, physics_properties);
            asset->mbPhysicsTransformOutOfDate = true;
            asset->SetPhysicsEnabled(true);

            asset->PrintDebug();
        });
}


void FxObject::PhysicsCreateMesh(FxRef<FxPrimitiveMesh> custom_physics_mesh, PhMotionType motion_type,
                                 const PhProperties& physics_properties)
{
    OnLoaded(
        [custom_physics_mesh, motion_type, physics_properties](FxTSRef<AxBase> base_asset)
        {
            FxTSRef<FxObject> asset = base_asset;

            FxRef<FxPrimitiveMesh> physics_mesh { nullptr };
            physics_mesh = custom_physics_mesh ? custom_physics_mesh : asset->pMesh;

            FxAssert(physics_mesh.IsValid());

            asset->Physics.CreateMeshBody(*physics_mesh, motion_type, physics_properties);
            asset->mbPhysicsTransformOutOfDate = true;
            asset->SetPhysicsEnabled(true);

            asset->PrintDebug();
        });
}

void FxObject::OnAttached(FxScene* scene)
{
    // When the object is attached to the scene, enable physics if the physics object is active.
    if (Physics.mbHasPhysicsBody) {
        mbPhysicsEnabled = gPhysics->GetBodyInterface().IsActive(Physics.GetBodyId());
    }
}


// void FxObject::PhysicsCreate(PhObject::Flags flags, PhMotionType moititype, const PhProperties& properties)
// {
//     Dimensions = pMesh->VertexList.CalculateDimensionsFromPositions();

//     FxVec3f scaled_dimensions = Dimensions * (mScale * 0.5);

//     Physics.CreatePhysicsBody(scaled_dimensions, mPosition, flags, type, properties);
//     mbPhysicsEnabled = gPhysics->GetBodyInterface().IsActive(Physics.GetBodyId());
// }

void FxObject::SetGraphicsPipeline(RxPipeline* pipeline, bool update_children)
{
    OnLoaded(
        [&pipeline, update_children](FxTSRef<AxBase> base_asset)
        {
            FxTSRef<FxObject> asset = base_asset;

            if (asset->pMaterial) {
                asset->pMaterial->pPipeline = pipeline;
            }

            if (update_children) {
                for (FxTSRef<FxObject>& node : asset->AttachedNodes) {
                    if (!node->pMaterial) {
                        continue;
                    }

                    node->pMaterial->pPipeline = pipeline;
                }
            }
        });
}

void FxObject::UpdateAnimation() {}

void FxObject::MakeInstanceOf(const FxTSRef<FxObject>& source_ref)
{
    FxRefContext<FxTSRef<FxObject>> source_ctx(source_ref);
    FxObject& source = source_ctx.Get();

    FxAssertMsg((source.mInstanceSlots - source.mInstanceSlotsInUse) > 0,
                "Object has no instance slots remaining! Did you reserve any instances on the source object?");

    gObjectManager->FreeObjectId(ObjectId);

    mpInstanceSource = source_ref;
    mbIsInstance = true;

    ++source.mInstanceSlotsInUse;

    ObjectId = source.ObjectId + source.mInstanceSlotsInUse;

    // Acquire the transformations from the source object
}

void FxObject::ReserveInstances(uint32 num)
{
    ObjectId = gObjectManager->ReserveInstances(ObjectId, num);
    mInstanceSlots = num;
    mInstanceSlotsInUse = 0;
}


void FxObject::Render(const FxCamera& camera)
{
    RxFrameData* frame = gRenderer->GetFrame();

    if (pMaterial && !pMaterial->bIsBuilt) {
        pMaterial->Build();
        return;
    }

    UpdateIfOutOfDate();

    FxDrawPushConstants push_constants {};

    push_constants.ObjectId = ObjectId;

    memcpy(push_constants.CameraMatrix, camera.GetCameraMatrix(mObjectLayer).RawData, sizeof(FxMat4f));

    if (pMaterial) {
        push_constants.MaterialIndex = pMaterial->GetMaterialIndex();
    }

    if (pMaterial && CheckIfReady()) {
        vkCmdPushConstants(frame->CommandBuffer.CommandBuffer, pMaterial->pPipeline->Layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),
                           &push_constants);

        RenderMesh();
    }

    // If there are no attached nodes, break early
    if (AttachedNodes.IsEmpty()) {
        return;
    }

    // If there are attached objects, then iterate through each and render each
    // This will use the MVP and uniforms from above
    for (FxTSRef<FxObject>& obj : AttachedNodes) {
        if (!obj->pMaterial) {
            continue;
        }

        if (!obj->pMaterial->bIsBuilt) {
            obj->pMaterial->Build();
        }

        obj->UpdateIfOutOfDate();

        push_constants.ObjectId = ObjectId;
        push_constants.MaterialIndex = obj->pMaterial->GetMaterialIndex();

        vkCmdPushConstants(frame->CommandBuffer.CommandBuffer, obj->pMaterial->pPipeline->Layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),
                           &push_constants);

        obj->RenderMesh();
    }
}

void FxObject::RenderUnlit(const FxCamera& camera)
{
    if (!GetRenderUnlit()) {
        return;
    }

    UpdateIfOutOfDate();

    if (pMaterial && !pMaterial->bIsBuilt) {
        pMaterial->Build();
        return;
    }

    if (pMaterial && !CheckIfReady()) {
        return;
    }


    FxDrawPushConstants push_constants {};
    memcpy(push_constants.CameraMatrix, camera.GetCameraMatrix(GetObjectLayer()).RawData, sizeof(FxMat4f));

    RxCommandBuffer& cmd = gRenderer->GetFrame()->CommandBuffer;
    RxPipeline& pipeline = gRenderer->pDeferredRenderer->PlUnlit;

    pipeline.Bind(cmd);


    if (pMaterial) {
        push_constants.ObjectId = ObjectId;
        push_constants.MaterialIndex = pMaterial->GetMaterialIndex();

        vkCmdPushConstants(cmd.Get(), pipeline.Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(push_constants), &push_constants);


        bool material_bound = pMaterial->BindWithPipeline(cmd, pipeline, false);
        if (!material_bound) {
            FxLogWarning("Material not bound!");
            return;
        }

        gObjectManager->mObjectBufferDS.BindWithOffset(2, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline,
                                                       gObjectManager->GetBaseOffset());

        RenderPrimitive(cmd);
    }


    gObjectManager->mObjectBufferDS.BindWithOffset(2, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline,
                                                   gObjectManager->GetBaseOffset());

    if (AttachedNodes.IsEmpty()) {
        return;
    }


    for (FxTSRef<FxObject>& obj : AttachedNodes) {
        if (!obj->pMaterial) {
            continue;
        }

        obj->Update();

        obj->UpdateIfOutOfDate();

        if (obj->pMaterial && !obj->pMaterial->bIsBuilt) {
            obj->pMaterial->Build();
            return;
        }

        if (!obj->CheckIfReady()) {
            return;
        }

        push_constants.ObjectId = ObjectId;
        push_constants.MaterialIndex = obj->pMaterial->GetMaterialIndex();

        vkCmdPushConstants(cmd.Get(), pipeline.Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(push_constants), &push_constants);


        bool material_bound = obj->pMaterial->BindWithPipeline(cmd, pipeline, false);
        if (!material_bound) {
            FxLogWarning("Material not bound!");
            return;
        }

        obj->RenderPrimitive(cmd);
    }
}


void FxObject::RenderPrimitive(const RxCommandBuffer& cmd)
{
    if (pMesh) {
        pMesh->Render(cmd, (mInstanceSlotsInUse + 1));
    }

    if (AttachedNodes.IsEmpty()) {
        return;
    }

    for (const FxTSRef<FxObject>& node : AttachedNodes) {
        if (node->pMesh) {
            node->pMesh->Render(cmd, (node->mInstanceSlotsInUse + 1)); // + 1 for source object!
        }
    }
}

void FxObject::RenderMesh()
{
    if (!CheckIfReady()) {
        return;
    }

    if (!pMaterial) {
        return;
    }

    RxFrameData* frame = gRenderer->GetFrame();

    RxCommandBuffer& cmd = frame->CommandBuffer;

    pMaterial->Bind(&cmd);

    gObjectManager->mObjectBufferDS.BindWithOffset(2, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pMaterial->pPipeline,
                                                   gObjectManager->GetBaseOffset());

    if (pMesh) {
        pMesh->Render(cmd, (mInstanceSlotsInUse + 1)); // + 1 for source object
    }
}

void FxObject::Update()
{
    if (mbPhysicsEnabled) {
        if (mbPhysicsTransformOutOfDate) {
            Physics.Teleport(mPosition, mRotation);
            mbPhysicsTransformOutOfDate = false;
        }

        SyncObjectWithPhysics();
    }
}


void FxObject::AttachObject(const FxTSRef<FxObject>& object)
{
    if (AttachedNodes.IsEmpty()) {
        AttachedNodes.Create(8);
    }

    Dimensions += object->Dimensions;

    AttachedNodes.Insert(object);
}

void FxObject::SyncObjectWithPhysics()
{
    if ((!mPosition.IsCloseTo(Physics.GetPosition()) || !mRotation.IsCloseTo(Physics.GetRotation()))) {
        mPosition = Physics.GetPosition();
        mRotation = Physics.GetRotation();

        MarkMatrixOutOfDate();
    }
}

void FxObject::SetRenderUnlit(const bool value)
{
    if (value) {
        SetGraphicsPipeline(&gRenderer->pDeferredRenderer->PlUnlit);
    }
    else {
        SetGraphicsPipeline(&gRenderer->pDeferredRenderer->PlGeometryWithNormalMaps);
    }

    mbRenderUnlit = value;
}


void FxObject::SetPhysicsEnabled(bool enabled)
{
    if (!Physics.mbHasPhysicsBody) {
        FxLogWarning("Object does not have physics body!");
        return;
    }

    if (enabled) {
        FxLogInfo("Activate physics body");
        gPhysics->GetBodyInterface().ActivateBody(Physics.GetBodyId());
    }
    else {
        FxLogInfo("Deactivate physics body");
        gPhysics->GetBodyInterface().DeactivateBody(Physics.GetBodyId());
    }

    mbPhysicsEnabled = enabled;
}

void FxObject::PrintDebug() const
{
    FxLogInfo("FxObject '{}' (Id={}) {{", Name.Get(), ObjectId);
    FxLogInfo("\tPos={}, Rot={}, Scale={}, Dim={}", mPosition, mRotation, mScale, Dimensions);
    FxLogInfo("\tHasPhys?={}, Enabled?={}, Type={}", Physics.mbHasPhysicsBody, mbPhysicsEnabled,
              Physics.GetMotionType() == PhMotionType::eStatic ? "Static" : "Dynamic");
    FxLogInfo("\tIsInstance?={}, ReadyToRender?={}, ShadowCaster?={}, Skinned?={}", mbIsInstance, mbReadyToRender,
              mbIsShadowCaster, pMesh && pMesh->VertexList.IsSkinned());
    FxLogInfo("}}");
}


void FxObject::Destroy()
{
    if (pMesh) {
        pMesh->Destroy();
    }
    if (pMaterial) {
        pMaterial->Destroy();
    }

    Physics.DestroyPhysicsBody();

    if (!AttachedNodes.IsEmpty()) {
        for (FxTSRef<FxObject>& obj : AttachedNodes) {
            obj->Destroy();
        }
    }

    mbReadyToRender = false;
    bIsUploadedToGpu = false;
}

FxObject::~FxObject() { Destroy(); }
