#include "glfw_engine.h"
#include "game.h"
#include "scene/node.h"
#include "util.h"
#include "asset/asset_manager.h"
#include "asset/text_loader.h"
#include "asset/byte_reader.h"
#include "rendering/shader.h"
#include "rendering/environment.h"
#include "rendering/texture.h"
#include "rendering/framebuffer_2d.h"
#include "rendering/framebuffer_cube.h"
#include "rendering/shaders/lighting_shader.h"
#include "rendering/shader_manager.h"
#include "rendering/shadow/pssm_shadow_mapping.h"
#include "audio/audio_manager.h"
#include "animation/skeleton_control.h"
#include "math/bounding_box.h"

#include "rendering/camera/fps_camera.h"

#include "util/mesh_factory.h"

#include "system/sdl_system.h"
#include "system/debug.h"
#include "rendering/backend/renderer_instance.h"
#include "rendering/backend/renderer_descriptor_set.h"
#include "rendering/backend/renderer_image.h"
#include "rendering/backend/renderer_fbo.h"
#include "rendering/backend/renderer_render_pass.h"

#include <rendering/v2/engine.h>
#include <rendering/v2/components/node.h>
#include <rendering/v2/components/bone.h>
#include <rendering/v2/asset/model_loaders/obj_model_loader.h>

#include "rendering/probe/envmap/envmap_probe_control.h"

#include "terrain/terrain_shader.h"

/* Post */
#include "rendering/postprocess/filters/gamma_correction_filter.h"
#include "rendering/postprocess/filters/ssao_filter.h"
#include "rendering/postprocess/filters/deferred_rendering_filter.h"
#include "rendering/postprocess/filters/bloom_filter.h"
#include "rendering/postprocess/filters/depth_of_field_filter.h"
#include "rendering/postprocess/filters/fxaa_filter.h"
#include "rendering/postprocess/filters/default_filter.h"

/* Extra */
#include "rendering/skydome/skydome.h"
#include "rendering/skybox/skybox.h"
#include "terrain/noise_terrain/noise_terrain_control.h"

/* Physics */
#include "physics/physics_manager.h"
#include "physics/rigid_body.h"
#include "physics/box_physics_shape.h"
#include "physics/sphere_physics_shape.h"
#include "physics/plane_physics_shape.h"

/* Particles */
#include "particles/particle_renderer.h"
#include "particles/particle_emitter_control.h"

/* Misc. Controls */
#include "controls/bounding_box_control.h"
#include "controls/camera_follow_control.h"
// #include "rendering/renderers/bounding_box_renderer.h"

/* UI */
#include "rendering/ui/ui_object.h"
#include "rendering/ui/ui_button.h"
#include "rendering/ui/ui_text.h"

/* Populators */
#include "terrain/populators/populator.h"

#include "util/noise_factory.h"
#include "util/img/write_bitmap.h"
#include "util/enum_options.h"
#include "util/mesh_factory.h"

#include "rendering/probe/gi/gi_probe_control.h"
#include "rendering/probe/spherical_harmonics/spherical_harmonics_control.h"
#include "rendering/shaders/gi/gi_voxel_debug_shader.h"
#include "rendering/shadow/shadow_map_control.h"

#include "asset/fbom/fbom.h"
#include "asset/byte_writer.h"

#include "util/profile.h"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cmath>
#include <thread>

using namespace hyperion;


#define HYPERION_VK_TEST_IMAGE_STORE 1
#define HYPERION_VK_TEST_VISUALIZE_OCTREE 0

int main()
{
    using namespace hyperion::renderer;

    std::string base_path = HYP_ROOT_DIR;
    AssetManager::GetInstance()->SetRootDir(base_path + "/res/");


    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 768);
    system.SetCurrentWindow(window);

    SystemEvent event;

    v2::Engine engine(system, "My app");


    std::vector<std::shared_ptr<Texture2D>> cubemap_faces;
    cubemap_faces.resize(6);

    cubemap_faces[0] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/posx.jpg");
    cubemap_faces[1] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/negx.jpg");
    cubemap_faces[2] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/posy.jpg");
    cubemap_faces[3] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/negy.jpg");
    cubemap_faces[4] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/posz.jpg");
    cubemap_faces[5] = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/Lycksele3/negz.jpg");

    size_t cubemap_face_bytesize = cubemap_faces[0]->GetWidth() * cubemap_faces[0]->GetHeight() * Texture::NumComponents(cubemap_faces[0]->GetFormat());

    unsigned char *bytes = new unsigned char[cubemap_face_bytesize * 6];

    for (int i = 0; i < cubemap_faces.size(); i++) {
        std::memcpy(&bytes[i * cubemap_face_bytesize], cubemap_faces[i]->GetBytes(), cubemap_face_bytesize);
    }

    auto cubemap = engine.resources.textures.Add(std::make_unique<v2::TextureCube>(
        Extent2D{
            uint32_t(cubemap_faces[0]->GetWidth()),
            uint32_t(cubemap_faces[0]->GetHeight())
        },
        Image::InternalFormat(cubemap_faces[0]->GetInternalFormat()),
        Image::FilterMode::TEXTURE_FILTER_LINEAR,
        Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER,
        bytes
    ));

    delete[] bytes;


    auto texture = engine.resources.textures.Add(
        engine.assets.Load<v2::Texture>(base_path + "/res/textures/dirt.jpg")
    );

    auto texture2 = engine.resources.textures.Add(
        engine.assets.Load<v2::Texture>(base_path + "/res/textures/dummy.jpg")
    );

#if HYPERION_VK_TEST_IMAGE_STORE
    renderer::Image *image_storage = new renderer::StorageImage(
        Extent3D{512, 512, 1},
        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Image::Type::TEXTURE_TYPE_2D,
        nullptr
    );

    ImageView image_storage_view;
#endif

    engine.Initialize();

    auto [zombie, sponza, cube_obj] = engine.assets.Load<v2::Node>(
        base_path + "/res/models/ogrexml/dragger_Body.mesh.xml",
        base_path + "/res/models/sponza/sponza.obj",
        base_path + "/res/models/cube.obj"
    );


    zombie->Scale(0.35f);

    sponza->Scale(0.1f);
    //sponza->Rotate(Quaternion({1, 0, 0}, MathUtil::DegToRad(90.0f)));
    sponza->Update(&engine);


    Device *device = engine.GetInstance()->GetDevice();

#if HYPERION_VK_TEST_IMAGE_STORE
    auto *descriptor_set_globals = engine.GetInstance()->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL);


    descriptor_set_globals
        ->AddDescriptor<ImageStorageDescriptor>(16)
        ->AddSubDescriptor({ .image_view = &image_storage_view });

    HYPERION_ASSERT_RESULT(image_storage->Create(
        device,
        engine.GetInstance(),
        Image::LayoutTransferState<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL>{}
    ));

    HYPERION_ASSERT_RESULT(image_storage_view.Create(device, image_storage));
#endif

    engine.PrepareSwapchain();
    
    auto mirror_shader = engine.resources.shaders.Add(std::make_unique<v2::Shader>(
        std::vector<v2::SubShader>{
            {
                ShaderModule::Type::VERTEX, {
                    FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/vert.spv").Read(),
                    {.name = "main vert"}
                }
            },
            {
                ShaderModule::Type::FRAGMENT, {
                    FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/forward_frag.spv").Read(),
                    {.name = "forward frag"}
                }
            }
        }
    ));

    float timer = 0.0;

    auto *input_manager = new InputManager(window);
    input_manager->SetWindow(window);

    auto scene = engine.resources.scenes.Add(std::make_unique<v2::Scene>(
        std::make_unique<FpsCamera>(
            input_manager,
            window,
            1024,
            768,
            70.0f,
            0.05f,
            250.0f
        )
    ));

    bool running = true;

    Frame *frame = nullptr;

    uint64_t tick_now = SDL_GetPerformanceCounter();
    uint64_t tick_last = 0;
    double delta_time = 0;

#if HYPERION_VK_TEST_IMAGE_STORE
    /* Compute */
    v2::ComputePipeline::ID compute_pipeline_id = engine.resources.compute_pipelines.Add(
        &engine,
        std::make_unique<v2::ComputePipeline>(
            engine.resources.shaders.Add(std::make_unique<v2::Shader>(
                std::vector<v2::SubShader>{
                    { ShaderModule::Type::COMPUTE, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/imagestore.comp.spv").Read()}}
                }
            ))
        )
    );

    auto compute_command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_PRIMARY);

    SemaphoreChain compute_semaphore_chain({}, { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT });
    AssertThrow(compute_semaphore_chain.Create(engine.GetInstance()->GetDevice()));

    HYPERION_ASSERT_RESULT(compute_command_buffer->Create(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetComputeCommandPool()));
    
    for (size_t i = 0; i < engine.GetInstance()->GetFrameHandler()->NumFrames(); i++) {
        /* Wait for compute to finish */
        compute_semaphore_chain.SignalsTo(engine.GetInstance()->GetFrameHandler()
            ->GetPerFrameData()[i].Get<Frame>()
            ->GetPresentSemaphores());
    }
#endif
    

    PerFrameData<CommandBuffer, Semaphore> per_frame_data(engine.GetInstance()->GetFrameHandler()->NumFrames());

    for (uint32_t i = 0; i < per_frame_data.NumFrames(); i++) {
        auto cmd_buffer = std::make_unique<CommandBuffer>(CommandBuffer::COMMAND_BUFFER_SECONDARY);
        AssertThrow(cmd_buffer->Create(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsQueue().command_pool));

        per_frame_data[i].Set<CommandBuffer>(std::move(cmd_buffer));
    }

    v2::Node new_root("root");

    auto mat1 = engine.resources.materials.Add(std::make_unique<v2::Material>());
    mat1->SetParameter(v2::Material::MATERIAL_KEY_ALBEDO, v2::Material::Parameter(Vector4{ 1.0f, 0.0f, 0.0f, 0.5f }));
    mat1->SetTexture(v2::Material::MATERIAL_TEXTURE_ALBEDO_MAP, texture.Acquire());
    mat1.Init();

    auto mat2 = engine.resources.materials.Add(std::make_unique<v2::Material>());
    mat2->SetParameter(v2::Material::MATERIAL_KEY_ALBEDO, v2::Material::Parameter(Vector4{ 0.0f, 0.0f, 1.0f, 1.0f }));
    mat2->SetTexture(v2::Material::MATERIAL_TEXTURE_ALBEDO_MAP, texture2.Acquire());
    mat2.Init();

    auto skybox_material = engine.resources.materials.Add(std::make_unique<v2::Material>());
    skybox_material->SetParameter(v2::Material::MATERIAL_KEY_ALBEDO, v2::Material::Parameter(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));
    skybox_material->SetTexture(v2::Material::MATERIAL_TEXTURE_ALBEDO_MAP, cubemap.Acquire());
    skybox_material.Init();

    auto translucent_material = engine.resources.materials.Add(std::make_unique<v2::Material>());
    translucent_material->SetParameter(v2::Material::MATERIAL_KEY_ALBEDO, v2::Material::Parameter(Vector4{ 0.0f, 1.0f, 0.0f, 0.2f }));
   // mat1.Init();

    //v2::Spatial *cube_spatial{};
    auto cube_spatial = engine.resources.spatials.Acquire(cube_obj->GetChild(0)->GetSpatial());
    auto zombie_spatial = engine.resources.spatials.Acquire(zombie->GetChild(0)->GetSpatial());

    v2::GraphicsPipeline::ID main_pipeline_id;
    {
        auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            mirror_shader.Acquire(),
            scene.Acquire(),
            engine.GetRenderList()[v2::GraphicsPipeline::BUCKET_OPAQUE].render_pass.Acquire(),
            v2::GraphicsPipeline::Bucket::BUCKET_OPAQUE
        );

        pipeline->AddSpatial(cube_spatial.Acquire());

        std::function<void(v2::Node *)> find_spatials = [&](v2::Node *node) {
            if (auto *spatial = node->GetSpatial()) {
                pipeline->AddSpatial(engine.resources.spatials.Acquire(spatial));
            }

            for (auto &child : node->GetChildren()) {
                find_spatials(child.get());
            }
        };

        find_spatials(sponza.get());
        
        main_pipeline_id = engine.AddGraphicsPipeline(std::move(pipeline));
    }

    v2::GraphicsPipeline::ID skybox_pipeline_id{};
    {

        auto shader = engine.resources.shaders.Add(std::make_unique<v2::Shader>(
            std::vector<v2::SubShader>{
                {ShaderModule::Type::VERTEX, { FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/skybox_vert.spv").Read()}},
                {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/skybox_frag.spv").Read()}}
            }
        ));

        auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            std::move(shader),
            scene.Acquire(),
            engine.GetRenderList().Get(v2::GraphicsPipeline::BUCKET_SKYBOX).render_pass.Acquire(),
            v2::GraphicsPipeline::Bucket::BUCKET_SKYBOX
        );
        pipeline->SetCullMode(CullMode::FRONT);
        pipeline->SetDepthTest(false);
        pipeline->SetDepthWrite(false);

        auto skybox_spatial = engine.resources.spatials.Acquire(cube_obj->GetChild(0)->GetSpatial());
        skybox_spatial->SetMaterial(std::move(skybox_material));
        pipeline->AddSpatial(std::move(skybox_spatial));
        
        skybox_pipeline_id = engine.AddGraphicsPipeline(std::move(pipeline));
    }

    v2::GraphicsPipeline::ID translucent_pipeline_id{};
    {
        auto pipeline = std::make_unique<v2::GraphicsPipeline>(
            mirror_shader.Acquire(),
            scene.Acquire(),
            engine.GetRenderList().Get(v2::GraphicsPipeline::BUCKET_TRANSLUCENT).render_pass.Acquire(),
            v2::GraphicsPipeline::Bucket::BUCKET_TRANSLUCENT
        );
        pipeline->SetBlendEnabled(true);

        auto zombie_node = std::make_unique<v2::Node>("monkey");
        zombie_node->SetSpatial(zombie_spatial.Acquire());
        pipeline->AddSpatial(zombie_spatial.Acquire());
        
        translucent_pipeline_id = engine.AddGraphicsPipeline(std::move(pipeline));
    }

#if HYPERION_VK_TEST_VISUALIZE_OCTREE
    v2::GraphicsPipeline::ID wire_pipeline_id{};

    auto pipeline = std::make_unique<v2::GraphicsPipeline>(
        mirror_shader.Acquire(),
        engine.GetRenderList().Get(v2::GraphicsPipeline::BUCKET_TRANSLUCENT).render_pass_id,
        v2::GraphicsPipeline::Bucket::BUCKET_TRANSLUCENT
    );
    pipeline->SetBlendEnabled(false);
    pipeline->SetFillMode(FillMode::LINE);
    pipeline->SetCullMode(CullMode::NONE);
    pipeline->SetTopology(Topology::TRIANGLES);
    pipeline->SetDepthTest(false);
    pipeline->SetDepthWrite(false);

    pipeline->GetVertexAttributes() |= MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES /* until we have dynamic shader compilation based on vertex attribs */
      | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS;

    std::unordered_map<v2::Octree *, v2::Spatial::ID> octree_debug_nodes;

    engine.GetOctree().GetCallbacks().on_insert_octant += [&](v2::Engine *engine, v2::Octree *octree, v2::Spatial *) {
        auto *pl = engine->GetGraphicsPipeline(wire_pipeline_id);

        auto mesh = MeshFactory::CreateCube(octree->GetAabb());

        auto spat = engine->resources.spatials.Add(std::make_unique<v2::Spatial>(
            engine->resources.meshes.Add(std::make_unique<v2::Mesh>(
                mesh->GetVertices(),
                mesh->GetIndices()
            )),
            pl->GetVertexAttributes(),
            Transform(),
            mesh->GetAABB(),
            engine->resources.materials.Get(v2::Material::ID{3})
        ));

        octree_debug_nodes[octree] = spat->GetId();

        pl->AddSpatial(std::move(spat));
    };

    engine.GetOctree().GetCallbacks().on_remove_octant += [&](v2::Engine *engine, v2::Octree *octree, v2::Spatial *) {
        auto *pl = engine->GetGraphicsPipeline(wire_pipeline_id);

        auto it = octree_debug_nodes.find(octree);

        if (it != octree_debug_nodes.end()) {
            vkQueueWaitIdle(engine->GetInstance()->GetGraphicsQueue().queue);
            pl->RemoveSpatial(it->second);

            octree_debug_nodes.erase(it);
        }
    };

    wire_pipeline_id = engine.AddGraphicsPipeline(std::move(pipeline));

#endif
    
    engine.Compile();

#if HYPERION_VK_TEST_IMAGE_STORE
    VkFence compute_fc;
    VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(engine.GetInstance()->GetDevice()->GetDevice(), &fence_info, nullptr, &compute_fc) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute fence!");
    }
#endif


    /* Shadow cam test */

    Matrix4 shadow_view;
    MatrixUtil::ToLookAt(shadow_view, {5, 5, 5}, {0, 0, 0}, {0, 1, 0});
    Matrix4 shadow_proj;
    MatrixUtil::ToOrtho(shadow_proj, -5, 5, -5, 5, -5, 5);

    engine.shader_globals->scenes.Set(1, {
        .view = shadow_view,
        .projection = shadow_proj,
        .camera_position = {5, 5, 5, 1},
        .light_direction = Vector4(Vector3(0.5f, 0.5f, 0.0f).Normalize(), 1.0f)
    });

    bool updated_descriptor = false;

    while (running) {
        tick_last = tick_now;
        tick_now = SDL_GetPerformanceCounter();
        delta_time = (double(tick_now)-double(tick_last)) / double(SDL_GetPerformanceFrequency());

        while (SystemSDL::PollEvent(&event)) {
            input_manager->CheckEvent(&event);
            switch (event.GetType()) {
                case SystemEventType::EVENT_SHUTDOWN:
                    running = false;
                    break;
                default:
                    break;
            }
        }

        timer += delta_time;

        scene->Update(&engine, delta_time);

        HYPERION_ASSERT_RESULT(engine.GetInstance()->GetFrameHandler()->PrepareFrame(
            engine.GetInstance()->GetDevice(),
            engine.GetInstance()->GetSwapchain()
        ));


        frame = engine.GetInstance()->GetFrameHandler()->GetCurrentFrameData().Get<Frame>();
        const uint32_t frame_index = engine.GetInstance()->GetFrameHandler()->GetCurrentFrameIndex();

#if HYPERION_VK_TEST_IMAGE_STORE
        auto *compute_pipeline = engine.resources.compute_pipelines[compute_pipeline_id];
        compute_pipeline->Get().push_constants.counter_x = std::sin(timer) * 20.0f;
        compute_pipeline->Get().push_constants.counter_y = std::cos(timer) * 20.0f;

        /* Compute */
        vkWaitForFences(engine.GetInstance()->GetDevice()->GetDevice(),
            1, &compute_fc, true, UINT64_MAX);
        vkResetFences(engine.GetInstance()->GetDevice()->GetDevice(),
            1, &compute_fc);
        
        compute_command_buffer->Reset(engine.GetInstance()->GetDevice());
        HYPERION_ASSERT_RESULT(compute_command_buffer->Record(engine.GetInstance()->GetDevice(), nullptr, [&](CommandBuffer *cmd) {
            compute_pipeline->Dispatch(&engine, cmd, {8, 8, 1});

            HYPERION_RETURN_OK;
        }));
        HYPERION_ASSERT_RESULT(compute_command_buffer->SubmitPrimary(
            engine.GetInstance()->GetComputeQueue().queue,
            compute_fc,
            &compute_semaphore_chain
        ));
#endif

        Transform transform(Vector3(-4, -7, -2), Vector3(0.35f), Quaternion());

        engine.GetOctree().CalculateVisibility(scene.ptr);

        sponza->Update(&engine);

        zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("head")->m_pose_transform.SetRotation(Quaternion({0, 1, 0}, timer * 0.35f));
        zombie->GetChild(0)->GetSpatial()->GetSkeleton()->FindBone("head")->UpdateWorldTransform();
        zombie->GetChild(0)->GetSpatial()->GetSkeleton()->SetShaderDataState(v2::ShaderDataState::DIRTY);
        zombie->SetLocalTransform(transform);
        zombie->Update(&engine);

        cube_obj->SetLocalTranslation(scene->GetCamera()->GetTranslation());
        cube_obj->Update(&engine);

        /* Only update sets that are double - buffered so we don't
         * end up updating data that is in use by the gpu
         */
        engine.UpdateDescriptorData(frame_index);
        
        HYPERION_ASSERT_RESULT(frame->BeginCapture());
        engine.RenderShadows(frame->GetCommandBuffer(), frame_index);
        engine.RenderDeferred(frame->GetCommandBuffer(), frame_index);
        engine.RenderPostProcessing(frame->GetCommandBuffer(), frame_index);


#if HYPERION_VK_TEST_IMAGE_STORE
        /* TODO: where should this go? find a way to abstract this */
        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        // We won't be changing the layout of the image
        image_memory_barrier.oldLayout        = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.newLayout        = VK_IMAGE_LAYOUT_GENERAL;
        image_memory_barrier.image            = image_storage->GetGPUImage()->image;
        image_memory_barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        image_memory_barrier.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT;
        image_memory_barrier.dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;
        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkCmdPipelineBarrier(
            frame->GetCommandBuffer()->GetCommandBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &image_memory_barrier);
#endif

        engine.RenderSwapchain(frame->GetCommandBuffer());

        HYPERION_ASSERT_RESULT(frame->EndCapture());
        HYPERION_ASSERT_RESULT(frame->Submit(&engine.GetInstance()->GetGraphicsQueue()));
        
        engine.GetInstance()->GetFrameHandler()->PresentFrame(&engine.GetInstance()->GetGraphicsQueue(), engine.GetInstance()->GetSwapchain());
        engine.GetInstance()->GetFrameHandler()->NextFrame();

    }

    AssertThrow(engine.GetInstance()->GetDevice()->Wait());

    v2::PostEffect::full_screen_quad.reset();// have to do this here for now or else buffer does not get cleared before device is deleted

    zombie.reset();
    cube_obj.reset();

#if HYPERION_VK_TEST_IMAGE_STORE
    HYPERION_ASSERT_RESULT(image_storage_view.Destroy(device));
    HYPERION_ASSERT_RESULT(image_storage->Destroy(device));

    delete image_storage;

#endif
    

    //delete cubemap;

    for (size_t i = 0; i < per_frame_data.NumFrames(); i++) {
        per_frame_data[i].Get<CommandBuffer>()->Destroy(engine.GetInstance()->GetDevice(), engine.GetInstance()->GetGraphicsCommandPool());
    }
    per_frame_data.Reset();

#if HYPERION_VK_TEST_IMAGE_STORE
    compute_command_buffer->Destroy(device, engine.GetInstance()->GetComputeCommandPool());
    compute_semaphore_chain.Destroy(engine.GetInstance()->GetDevice());

    vkDestroyFence(engine.GetInstance()->GetDevice()->GetDevice(), compute_fc, VK_NULL_HANDLE);
#endif


    engine.Destroy();

    delete window;

    return 0;
}
