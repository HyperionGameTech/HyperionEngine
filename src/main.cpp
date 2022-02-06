#include "glfw_engine.h"
#include "gl_util.h"
#include "game.h"
#include "entity.h"
#include "util.h"
#include "asset/asset_manager.h"
#include "asset/text_loader.h"
#include "rendering/mesh.h"
#include "rendering/shader.h"
#include "rendering/environment.h"
#include "rendering/camera/ortho_camera.h"
#include "rendering/camera/perspective_camera.h"
#include "rendering/camera/fps_camera.h"
#include "rendering/texture.h"
#include "rendering/framebuffer_2d.h"
#include "rendering/framebuffer_cube.h"
#include "rendering/shaders/lighting_shader.h"
#include "rendering/shaders/vegetation_shader.h"
#include "rendering/shaders/fur_shader.h"
#include "rendering/shaders/post_shader.h"
#include "rendering/shader_manager.h"
#include "rendering/shadow/shadow_mapping.h"
#include "rendering/shadow/pssm_shadow_mapping.h"
#include "util/shader_preprocessor.h"
#include "util/mesh_factory.h"
#include "util/aabb_factory.h"
#include "audio/audio_manager.h"
#include "audio/audio_source.h"
#include "audio/audio_control.h"
#include "animation/bone.h"
#include "animation/skeleton_control.h"
#include "math/bounding_box.h"

#include "rendering/probe/probe_renderer.h"

/* Post */
#include "rendering/postprocess/filters/gamma_correction_filter.h"
#include "rendering/postprocess/filters/ssao_filter.h"
#include "rendering/postprocess/filters/deferred_rendering_filter.h"
#include "rendering/postprocess/filters/bloom_filter.h"
#include "rendering/postprocess/filters/depth_of_field_filter.h"
#include "rendering/postprocess/filters/fxaa_filter.h"
#include "rendering/postprocess/filters/shadertoy_filter.h"

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

#include "rendering/gi/gi_probe_control.h"
#include "rendering/shaders/gi/gi_voxel_debug_shader.h"

/* Standard library */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <string>
#include <math.h>

using namespace hyperion;

class SceneEditor : public Game {
public:
    Camera *cam;
    PssmShadowMapping *shadows;

    std::vector<std::shared_ptr<Entity>> m_raytested_entities;
    std::unordered_map<HashCode_t, std::shared_ptr<Entity>> m_hit_to_entity;
    std::shared_ptr<Entity> m_selected_node;
    bool m_dragging_node = false;
    float m_dragging_timer = 0.0f;
    bool m_left_click_left = false;
    RaytestHit m_ray_hit;
    bool m_is_ray_hit = false;

    std::shared_ptr<ui::UIText> m_selected_node_text;
    std::shared_ptr<ui::UIButton> m_rotate_mode_btn;

    std::shared_ptr<Entity> top;

    SceneEditor(const RenderWindow &window)
        : Game(window)
    {
        std::srand(std::time(nullptr));

        ShaderProperties defines;
    }

    ~SceneEditor()
    {
        delete shadows;
        delete cam;

        AudioManager::Deinitialize();
    }

    void InitParticleSystem()
    {
        ParticleConstructionInfo particle_generator_info;
        particle_generator_info.m_origin_randomness = Vector3(5.0f, 0.5f, 5.0f);
        particle_generator_info.m_velocity = Vector3(0.5f, -3.0f, 0.5f);
        particle_generator_info.m_velocity_randomness = Vector3(1.3f, 0.0f, 1.3f);
        particle_generator_info.m_scale = Vector3(0.05);
        particle_generator_info.m_gravity = Vector3(0, -9, 0);
        particle_generator_info.m_max_particles = 300;
        particle_generator_info.m_lifespan = 1.0;
        particle_generator_info.m_lifespan_randomness = 1.5;

        auto particle_node = std::make_shared<Entity>();
        particle_node->SetName("Particle node");
        // particle_node->SetRenderable(std::make_shared<ParticleRenderer>(particle_generator_info));
        particle_node->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/textures/test_snowflake.png"));
        particle_node->AddControl(std::make_shared<ParticleEmitterControl>(cam, particle_generator_info));
        particle_node->SetLocalScale(Vector3(1.5));
        particle_node->AddControl(std::make_shared<BoundingBoxControl>());
        particle_node->SetLocalTranslation(Vector3(0, 165, 0));
        particle_node->AddControl(std::make_shared<CameraFollowControl>(cam, Vector3(0, 2, 0)));

        top->AddChild(particle_node);
    }

    std::shared_ptr<Cubemap> InitCubemap()
    {
        std::shared_ptr<Cubemap> cubemap(new Cubemap({
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posx.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negx.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posy.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negy.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/posz.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver/negz.jpg")
        }));

        if (!Environment::GetInstance()->ProbeEnabled()) {
            Environment::GetInstance()->SetGlobalCubemap(cubemap);
        }

        Environment::GetInstance()->SetGlobalIrradianceCubemap(std::shared_ptr<Cubemap>(new Cubemap({
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/posx.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/negx.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/posy.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/negy.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/posz.jpg"),
            AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/IceRiver_irradiance/negz.jpg")
        })));

        return cubemap;
    }

    void InitTestArea()
    {
        bool voxel_debug = false;

        auto mitsuba = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/mitsuba.obj");
        //living_room->Scale(0.05f);
        mitsuba->Move(Vector3(-4.5, 1.2, -4.5));
        mitsuba->Scale(2);
        mitsuba->GetChild(0)->GetMaterial().diffuse_color = Vector4(1.0);
        mitsuba->GetChild(0)->GetMaterial().SetParameter("Emissiveness", 60.0f);
        for (size_t i = 0; i < mitsuba->NumChildren(); i++) {
            if (mitsuba->GetChild(i)->GetRenderable() == nullptr) {
                continue;
            }

            if (voxel_debug)
                mitsuba->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
        }
        top->AddChild(mitsuba);

        /*auto sponza = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sponza/sponza.obj");
        sponza->Scale(Vector3(0.1f));
        //if (voxel_debug) {
            for (size_t i = 0; i < sponza->NumChildren(); i++) {
                sponza->GetChild(i)->GetMaterial().SetParameter("shininess", 0.1f);
                sponza->GetChild(i)->GetMaterial().SetParameter("roughness", 0.8f);
                if (sponza->GetChild(i)->GetRenderable() == nullptr) {
                    continue;
                }
                if (voxel_debug) {
                    sponza->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                }
            }
        //}
        top->AddChild(sponza);*/
        /*{

            auto street = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/street/street.obj");
            street->SetName("street");
            // street->SetLocalTranslation(Vector3(3.0f, -0.5f, -1.5f));
            street->Scale(0.6f);

            for (size_t i = 0; i < street->NumChildren(); i++) {
                if (voxel_debug)
                    street->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                street->GetChild(i)->GetMaterial().SetParameter("shininess", 0.3f);
                street->GetChild(i)->GetMaterial().SetParameter("roughness", 0.8f);
            }

            top->AddChild(street);
            street->UpdateTransform();
        }*/


        {
            auto model = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/conference/conference.obj");
            model->SetName("model");
            model->Scale(0.01f);

            for (size_t i = 0; i < model->NumChildren(); i++) {
                if (voxel_debug)
                    model->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                model->GetChild(i)->GetMaterial().SetParameter("shininess", 0.2f);
                model->GetChild(i)->GetMaterial().SetParameter("roughness", 0.8f);
            }

            top->AddChild(model);
            model->UpdateTransform();
        }

        for (int x = 0; x < 5; x++) {
            for (int z = 0; z < 5; z++) {
                Vector3 box_position = Vector3(((float(x) - 2.5) * 8), 3.0f, (float(z) - 2.5) * 8);
                auto box = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/sphere_hq.obj", true);
                box->Scale(0.7f);

                for (size_t i = 0; i < box->NumChildren(); i++) {
                    Vector3 col = Vector3(
                        MathUtil::Random(0.4f, 1.80f),
                        MathUtil::Random(0.4f, 1.80f),
                        MathUtil::Random(0.4f, 1.80f)
                    ).Normalize();

                    box->GetChild(i)->GetMaterial().diffuse_color = Vector4(
                        col.x,
                        col.y,
                        col.z,
                        1.0f
                    );

                    // box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_albedo.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_height.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_ao.png"));
                    // box->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/steelplate/steelplate1_normal-ogl.png"));
                    box->GetChild(i)->GetMaterial().SetParameter("shininess", 0.25f);
                    box->GetChild(i)->GetMaterial().SetParameter("roughness", 0.8f);
                    if (voxel_debug)
                        box->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                   // box->GetChild(i)->GetMaterial().SetParameter("shininess", 0.8f);
                    //box->GetChild(i)->GetMaterial().SetParameter("roughness", 0.1f);
                }

                box->SetLocalTranslation(box_position);
                top->AddChild(box);
            }
        }
    }

    void PerformRaytest()
    {
        m_raytested_entities.clear();

        Ray ray;
        ray.m_direction = cam->GetDirection();
        ray.m_direction.Normalize();
        ray.m_position = cam->GetTranslation();


        using Intersection_t = std::pair<std::shared_ptr<Entity>, RaytestHit>;

        RaytestHit intersection;
        std::vector<Intersection_t> intersections = { { top, intersection } };

        while (true) {
            std::vector<Intersection_t> new_intersections;

            for (auto& it : intersections) {
                for (size_t i = 0; i < it.first->NumChildren(); i++) {
                    const auto& child = it.first->GetChild(i);

                    BoundingBox aabb = child->GetAABB();

                    if (aabb.IntersectRay(ray, intersection)) {
                        new_intersections.push_back({ child, intersection });
                    }
                }
            }

            if (new_intersections.empty()) {
                break;
            }

            intersections = new_intersections;
        }

        if (intersections.empty()) {
            return;
        }

        // RAY HIT POSITION / NORMAL

        RaytestHitList_t mesh_intersections;
        m_hit_to_entity.clear();

        for (Intersection_t& it : intersections) {
            // it.first->AddControl(std::make_shared<BoundingBoxControl>());
            m_raytested_entities.push_back(it.first);

            if (auto renderable = it.first->GetRenderable()) {
                RaytestHitList_t entity_hits;
                if (renderable->IntersectRay(ray, it.first->GetGlobalTransform(), entity_hits)) {
                    mesh_intersections.insert(mesh_intersections.end(), entity_hits.begin(), entity_hits.end());

                    for (auto& hit : entity_hits) {
                        m_hit_to_entity[hit.GetHashCode().Value()] = it.first;
                    }
                }

            }
        }

        if (mesh_intersections.empty()) {
            m_is_ray_hit = false;
            return;
        }

        std::sort(mesh_intersections.begin(), mesh_intersections.end(), [=](const RaytestHit& a, const RaytestHit& b) {
            return a.hitpoint.Distance(cam->GetTranslation()) < b.hitpoint.Distance(cam->GetTranslation());
        });

        m_ray_hit = mesh_intersections[0];
        m_is_ray_hit = true;
    }

    void Initialize()
    {
        ShaderManager::GetInstance()->SetBaseShaderProperties(ShaderProperties()
            .Define("NORMAL_MAPPING", true)
            .Define("SHADOW_MAP_RADIUS", 0.05f)
            .Define("SHADOW_PCF", true)
        );
        
        cam = new FpsCamera(
            GetInputManager(),
            &m_renderer->GetRenderWindow(),
            m_renderer->GetRenderWindow().GetScaledWidth(),
            m_renderer->GetRenderWindow().GetScaledHeight(),
            75.0f,
            0.5f,
            750.0f
        );

        shadows = new PssmShadowMapping(cam, 4, 120.0f);
        shadows->SetVarianceShadowMapping(true);

        Environment::GetInstance()->SetVCTEnabled(true);
        Environment::GetInstance()->SetShadowsEnabled(false);
        Environment::GetInstance()->SetNumCascades(4);
        Environment::GetInstance()->SetProbeEnabled(false);
        Environment::GetInstance()->GetProbeRenderer()->SetRenderShading(true);
        Environment::GetInstance()->GetProbeRenderer()->SetRenderTextures(true);
        Environment::GetInstance()->GetProbeRenderer()->GetProbe()->SetOrigin(Vector3(0, 0, 0));

        m_renderer->GetPostProcessing()->AddFilter<SSAOFilter>("ssao", 5);
        m_renderer->GetPostProcessing()->AddFilter<BloomFilter>("bloom", 40);
        //m_renderer->GetPostProcessing()->AddFilter<DepthOfFieldFilter>("depth of field", 50);
        m_renderer->GetPostProcessing()->AddFilter<GammaCorrectionFilter>("gamma correction", 999);
        m_renderer->GetPostProcessing()->AddFilter<FXAAFilter>("fxaa", 9999);
        m_renderer->SetDeferred(true);

        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(0.5).Normalize());

        /*for (int x = 0; x < Environment::GetInstance()->GetMaxPointLights() / 2; x++) {
            for (int z = 0; z < Environment::GetInstance()->GetMaxPointLights() / 2; z++) {
                Environment::GetInstance()->AddPointLight(std::make_shared<PointLight>(Vector3(x * 0.5f, -6.0f, z * 0.5f), Vector4(MathUtil::Random(0.0f, 1.0f), MathUtil::Random(0.0f, 1.0f), MathUtil::Random(0.0f, 1.0f), 1.0f), 2.0f));
            }
        }*/
        // Initialize root node
        top = std::make_shared<Entity>("top");

        auto gi_test_node = std::make_shared<Entity>("gi_test_node");
        gi_test_node->Move(Vector3(0, 5, 0));
        gi_test_node->AddControl(std::make_shared<GIProbeControl>(BoundingBox(Vector3(-25.0f), Vector3(25.0f))));
        top->AddChild(gi_test_node);


        cam->SetTranslation(Vector3(4, 0, 0));

        // Initialize particle system
        // InitParticleSystem();

        auto ui_text = std::make_shared<ui::UIText>("text_test", "Hyperion 0.1.0\n"
            "Press 1 to toggle shadows\n"
            "Press 2 to toggle deferred rendering\n"
            "Press 3 to toggle voxel cone tracing\n");
        ui_text->SetLocalTranslation2D(Vector2(-1.0, 1.0));
        ui_text->SetLocalScale2D(Vector2(30));
        top->AddChild(ui_text);
        GetUIManager()->RegisterUIObject(ui_text);

        auto fps_counter = std::make_shared<ui::UIText>("fps_coutner", "- FPS");
        fps_counter->SetLocalTranslation2D(Vector2(0.8, 1.0));
        fps_counter->SetLocalScale2D(Vector2(30));
        top->AddChild(fps_counter);
        GetUIManager()->RegisterUIObject(fps_counter);

        m_selected_node_text = std::make_shared<ui::UIText>("selected_node_text", "No object selected");
        m_selected_node_text->SetLocalTranslation2D(Vector2(-1.0, -0.8));
        m_selected_node_text->SetLocalScale2D(Vector2(15));
        top->AddChild(m_selected_node_text);
        GetUIManager()->RegisterUIObject(m_selected_node_text);

        m_rotate_mode_btn = std::make_shared<ui::UIButton>("rotate_mode");
        m_rotate_mode_btn->SetLocalTranslation2D(Vector2(-1.0, -0.6));
        m_rotate_mode_btn->SetLocalScale2D(Vector2(15));
        top->AddChild(m_rotate_mode_btn);
        GetUIManager()->RegisterUIObject(m_rotate_mode_btn);

        auto cm = InitCubemap();

        top->AddControl(std::make_shared<SkydomeControl>(cam));
        // top->AddControl(std::make_shared<SkyboxControl>(cam, nullptr));

        // shader = ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties());

        /*auto hydrant = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/FireHydrant/FireHydrantMesh.obj");
        hydrant->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/models/FireHydrant/fire_hydrant_Base_Color.png"));
        hydrant->GetChild(0)->GetMaterial().SetTexture("NormalMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/models/FireHydrant/fire_hydrant_Normal_OpenGL.png"));
        hydrant->GetChild(0)->GetMaterial().SetTexture("AoMap", AssetManager::GetInstance()->LoadFromFile<Texture>("res/models/FireHydrant/fire_hydrant_Mixed_AO.png"));
        hydrant->GetChild(0)->GetMaterial().SetParameter("Emissiveness", 3.5f);
        hydrant->Scale(Vector3(5.0f));
        top->AddChild(hydrant);*/

        InitTestArea();

        // auto ui_depth_view = std::make_shared<ui::UIObject>("fbo_preview_depth");
        // ui_depth_view->SetLocalTranslation2D(Vector2(0.8, -0.5));
        // ui_depth_view->SetLocalScale2D(Vector2(256));
        // top->AddChild(ui_depth_view);
        // GetUIManager()->RegisterUIObject(ui_depth_view);

        /*int total_cascades = Environment::GetInstance()->NumCascades();
        for (int x = 0; x < 2; x++) {
            for (int z = 0; z < 2; z++) {
                auto ui_fbo_view = std::make_shared<ui::UIObject>("fbo_preview_" + std::to_string(x * 2 + z));
                ui_fbo_view->GetMaterial().SetTexture("ColorMap", Environment::GetInstance()->GetShadowMap(x * 2 + z));
                ui_fbo_view->SetLocalTranslation2D(Vector2(0.7 + (double(x) * 0.2), -0.4 + (double(z) * -0.3)));
                ui_fbo_view->SetLocalScale2D(Vector2(256));
                top->AddChild(ui_fbo_view);
                GetUIManager()->RegisterUIObject(ui_fbo_view);

                total_cascades--;
                if (total_cascades == 0) {
                    break;
                }
            }

            if (total_cascades == 0) {
                break;
            }
        }*/

        auto ui_crosshair = std::make_shared<ui::UIObject>("crosshair");
        ui_crosshair->GetMaterial().SetTexture("ColorMap", AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/crosshair.png"));
        ui_crosshair->SetLocalTranslation2D(Vector2(0));
        ui_crosshair->SetLocalScale2D(Vector2(128));
        top->AddChild(ui_crosshair);
        GetUIManager()->RegisterUIObject(ui_crosshair);


        GetInputManager()->RegisterKeyEvent(KEY_1, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            Environment::GetInstance()->SetShadowsEnabled(!Environment::GetInstance()->ShadowsEnabled());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_2, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            m_renderer->SetDeferred(!m_renderer->IsDeferred());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_3, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            Environment::GetInstance()->SetVCTEnabled(!Environment::GetInstance()->VCTEnabled());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_ARROW_LEFT, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            if (m_selected_node == nullptr) {
                return;
            }

            // move the node in the left direction relative to the camera
            Vector3 lookat_vector = cam->GetTranslation() - m_selected_node->GetGlobalTranslation();
            lookat_vector.Normalize();

            Vector3 dir;

            if (fabs(lookat_vector.z) > fabs(lookat_vector.x)) {
                if (lookat_vector.z > 0.0f) {
                    dir = Vector3(-1.0f, 0.0f, 0.0f);
                } else {
                    dir = Vector3(1.0f, 0.0f, 0.0f);
                }
            } else {
                if (lookat_vector.x > 0.0f) {
                    dir = Vector3(0.0f, 0.0f, 1.0f);
                } else {
                    dir = Vector3(0.0f, 0.0f, -1.0f);
                }
            }

            m_selected_node->Move(dir);
        }));

        GetInputManager()->RegisterKeyEvent(KEY_ARROW_RIGHT, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            if (m_selected_node == nullptr) {
                return;
            }

            // move the node in the left direction relative to the camera
            Vector3 lookat_vector = cam->GetTranslation() - m_selected_node->GetGlobalTranslation();
            lookat_vector.Normalize();

            Vector3 dir;

            if (fabs(lookat_vector.z) > fabs(lookat_vector.x)) {
                if (lookat_vector.z > 0.0f) {
                    dir = Vector3(1.0f, 0.0f, 0.0f);
                }
                else {
                    dir = Vector3(-1.0f, 0.0f, 0.0f);
                }
            }
            else {
                if (lookat_vector.x > 0.0f) {
                    dir = Vector3(0.0f, 0.0f, -1.0f);
                }
                else {
                    dir = Vector3(0.0f, 0.0f, 1.0f);
                }
            }

            m_selected_node->Move(dir);
        }));


        /* {
            auto building = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/building2/building2.obj", true);
            building->SetLocalTranslation(Vector3(4, 2, 0));
            building->SetLocalScale(Vector3(0.5f));
            for (int i = 0; i < building->NumChildren(); i++) {
                //building->GetChild(i)->GetRenderable()->SetShader(shader);
                // building->GetChild(0)->GetMaterial().diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                building->GetChild(0)->GetMaterial().SetParameter("shininess", 0.5f);
                building->GetChild(0)->GetMaterial().SetParameter("roughness", 0.5f);
            }
            top->AddChild(building);
        }*/


        InputEvent raytest_event([=](bool pressed)
            {
                if (!pressed) {
                    return;
                }

                if (m_selected_node != nullptr) {
                    m_selected_node->RemoveControl(m_selected_node->GetControl<BoundingBoxControl>(0));
                }

                PerformRaytest();

                if (!m_is_ray_hit) {
                    return;
                }

                ex_assert(m_hit_to_entity.find(m_ray_hit.GetHashCode().Value()) != m_hit_to_entity.end());

                m_selected_node = m_hit_to_entity[m_ray_hit.GetHashCode().Value()];
                m_selected_node->AddControl(std::make_shared<BoundingBoxControl>());

                std::stringstream ss;
                ss << "Selected object: ";
                ss << m_selected_node->GetName();
                ss << " ";
                ss << m_selected_node->GetGlobalTranslation();
                
                m_selected_node_text->SetText(ss.str());

                /*auto cube = top->GetChild("cube");
                cube->SetGlobalTranslation(mesh_intersections[0].hitpoint);

                Matrix4 look_at;
                MatrixUtil::ToLookAt(look_at, mesh_intersections[0].normal, Vector3::UnitY());
                cube->SetGlobalRotation(Quaternion(look_at));*/
            });

        GetInputManager()->RegisterClickEvent(MOUSE_BTN_LEFT, raytest_event);
    }

    void Logic(double dt)
    {
        if (GetInputManager()->IsButtonDown(MouseButton::MOUSE_BTN_LEFT) && m_selected_node != nullptr) {
            //std::cout << "Left button down\n";
            if (!m_dragging_node) {
                m_dragging_timer += dt;

                if (m_dragging_timer >= 0.5f) {
                    std::cout << "Dragging " << m_selected_node->GetName() << "\n";
                    m_dragging_node = true;
                }
            } else {
                PerformRaytest();

                if (m_is_ray_hit) {
                    // check what it is intersecting with
                    auto intersected_with = m_hit_to_entity[m_ray_hit.GetHashCode().Value()];

                    ex_assert(intersected_with != nullptr);

                    std::cout << "Intersected with: " << intersected_with->GetName() << "\n";

                    if (intersected_with != m_selected_node) {
                        m_selected_node->SetGlobalTranslation(m_ray_hit.hitpoint);
                        m_selected_node->UpdateTransform();

                        std::stringstream ss;
                        ss << "Selected object: ";
                        ss << m_selected_node->GetName();
                        ss << " ";
                        ss << m_selected_node->GetGlobalTranslation();

                        m_selected_node_text->SetText(ss.str());
                    }
                }
            }
        } else {
            m_dragging_node = false;
            m_dragging_timer = 0.0f;
        }

        AudioManager::GetInstance()->SetListenerPosition(cam->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(cam->GetDirection(), cam->GetUpVector());

        cam->Update(dt);

        PhysicsManager::GetInstance()->RunPhysics(dt);

        top->Update(dt);
    }

    void Render()
    {
        m_renderer->Begin(cam, top.get());

        if (Environment::GetInstance()->ShadowsEnabled()) {
            Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
            // shadow_dir.SetY(-1.0f);
            shadows->SetOrigin(cam->GetTranslation());
            shadows->SetLightDirection(shadow_dir);
            shadows->Render(m_renderer);
        }

        // TODO: ProbeControl on top node
        if (Environment::GetInstance()->ProbeEnabled()) {
            Environment::GetInstance()->GetProbeRenderer()->SetOrigin(Vector3(cam->GetTranslation()));
            Environment::GetInstance()->GetProbeRenderer()->Render(m_renderer, cam);

            if (!Environment::GetInstance()->GetGlobalCubemap()) {
                Environment::GetInstance()->SetGlobalCubemap(
                    Environment::GetInstance()->GetProbeRenderer()->GetColorTexture()
                );
            }
        }
        m_renderer->Render(cam);
        m_renderer->End(cam, top.get());

        top->ClearPendingRemoval();
    }
};

int main()
{
    CoreEngine *engine = new GlfwEngine();
    CoreEngine::SetInstance(engine);

    auto *game = new SceneEditor(RenderWindow(1480, 1200, "Hyperion Demo"));

    engine->InitializeGame(game);

    delete game;
    delete engine;

    return 0;
}
