#include "glfw_engine.h"
#include "game.h"
#include "entity.h"
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

#include "asset/json/json.h"
#include "asset/json/lexer.h"
#include "asset/json/parser.h"

#include "system/sdl_system.h"
#include "system/debug.h"
#include "rendering/vulkan/vk_renderer.h"

#include "rendering/probe/envmap/envmap_probe_control.h"

/* Post */
#include "rendering/postprocess/filters/gamma_correction_filter.h"
#include "rendering/postprocess/filters/ssao_filter.h"
#include "rendering/postprocess/filters/deferred_rendering_filter.h"
#include "rendering/postprocess/filters/bloom_filter.h"
#include "rendering/postprocess/filters/depth_of_field_filter.h"
#include "rendering/postprocess/filters/fxaa_filter.h"
#include "rendering/postprocess/filters/shadertoy_filter.h"
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

#include "rendering/probe/gi/gi_probe_control.h"
#include "rendering/shaders/gi/gi_voxel_debug_shader.h"

#include "asset/fbom/fbom.h"
#include "asset/byte_writer.h"


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

    SceneEditor(const RenderWindow &window)
        : Game(window)
    {
        std::srand(std::time(nullptr));

        ShaderProperties defines;
    }

    ~SceneEditor()
    {
        delete shadows;

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
        particle_node->GetMaterial().SetTexture("DiffuseMap", AssetManager::GetInstance()->LoadFromFile<Texture>("textures/test_snowflake.png"));
        particle_node->AddControl(std::make_shared<ParticleEmitterControl>(GetCamera(), particle_generator_info));
        particle_node->SetLocalScale(Vector3(1.5));
        particle_node->AddControl(std::make_shared<BoundingBoxControl>());
        particle_node->SetLocalTranslation(Vector3(0, 165, 0));
        particle_node->AddControl(std::make_shared<CameraFollowControl>(GetCamera(), Vector3(0, 2, 0)));

        GetScene()->AddChild(particle_node);
    }

    std::shared_ptr<Cubemap> InitCubemap()
    {
        AssetManager *asset_manager = AssetManager::GetInstance();
        std::shared_ptr<Cubemap> cubemap(new Cubemap({
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver/posx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver/negx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver/posy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver/negy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver/posz.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver/negz.jpg")
        }));

        if (!ProbeManager::GetInstance()->EnvMapEnabled()) {
            Environment::GetInstance()->SetGlobalCubemap(cubemap);
        }

        Environment::GetInstance()->SetGlobalIrradianceCubemap(std::shared_ptr<Cubemap>(new Cubemap({
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver_irradiance/posx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver_irradiance/negx.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver_irradiance/posy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver_irradiance/negy.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver_irradiance/posz.jpg"),
            asset_manager->LoadFromFile<Texture2D>("textures/IceRiver_irradiance/negz.jpg")
        })));

        return cubemap;
    }

    void InitTestArea()
    {
        AssetManager *asset_manager = AssetManager::GetInstance();
        bool voxel_debug = false;

        auto mitsuba = asset_manager->LoadFromFile<Entity>("models/mitsuba.obj");
        //living_room->Scale(0.05f);
        mitsuba->Move(Vector3(-4.5, 1.2, -4.5));
        mitsuba->Scale(2);
        mitsuba->GetChild(0)->GetMaterial().diffuse_color = Vector4(1.0);
        mitsuba->GetChild(0)->GetMaterial().SetParameter(MATERIAL_PARAMETER_EMISSIVENESS, 60.0f);
        for (size_t i = 0; i < mitsuba->NumChildren(); i++) {
            if (mitsuba->GetChild(i)->GetRenderable() == nullptr) {
                continue;
            }

            if (voxel_debug)
                mitsuba->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
        }
        GetScene()->AddChild(mitsuba);

        auto sponza = asset_manager->LoadFromFile<Entity>("models/sponza/sponza.obj");
        sponza->SetName("sponza");
        sponza->Scale(Vector3(0.07f));
        //if (voxel_debug) {
            for (size_t i = 0; i < sponza->NumChildren(); i++) {
                sponza->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.2f);
                sponza->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                if (sponza->GetChild(i)->GetRenderable() == nullptr) {
                    continue;
                }
                if (voxel_debug) {
                    sponza->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                }
            }
        //}
        sponza->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 1.0f, 0.0f)));
        GetScene()->AddChild(sponza);
        return;
        {

            auto street = asset_manager->LoadFromFile<Entity>("models/street/street.obj");
            street->SetName("street");
            // street->SetLocalTranslation(Vector3(3.0f, -0.5f, -1.5f));
            street->Scale(0.6f);

            for (size_t i = 0; i < street->NumChildren(); i++) {
                if (voxel_debug)
                    street->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
            }

            GetScene()->AddChild(street);
            street->UpdateTransform();
        }


        {
            auto model = asset_manager->LoadFromFile<Entity>("models/conference/conference.obj");
            model->SetName("model");
            model->Scale(0.01f);

            for (size_t i = 0; i < model->NumChildren(); i++) {
                if (voxel_debug)
                    model->GetChild(i)->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelDebugShader>(ShaderProperties()));
                model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.2f);
                model->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
            }

            GetScene()->AddChild(model);
            model->UpdateTransform();
        }

        
    }

    void PerformRaytest()
    {
        m_raytested_entities.clear();

        Ray ray;
        ray.m_direction = GetCamera()->GetDirection();
        ray.m_direction.Normalize();
        ray.m_position = GetCamera()->GetTranslation();


        using Intersection_t = std::pair<std::shared_ptr<Entity>, RaytestHit>;

        RaytestHit intersection;
        std::vector<Intersection_t> intersections = { { GetScene(), intersection } };

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
            return a.hitpoint.Distance(GetCamera()->GetTranslation()) < b.hitpoint.Distance(GetCamera()->GetTranslation());
        });

        m_ray_hit = mesh_intersections[0];
        m_is_ray_hit = true;
    }

    void Initialize()
    {

        ShaderManager::GetInstance()->SetBaseShaderProperties(ShaderProperties()
            .Define("NORMAL_MAPPING", false)
            .Define("SHADOW_MAP_RADIUS", 0.05f)
            .Define("SHADOW_PCF", false)
        );

        AssetManager *asset_manager = AssetManager::GetInstance();

        shadows = new PssmShadowMapping(GetCamera(), 4, 120.0f);
        shadows->SetVarianceShadowMapping(true);

        Environment::GetInstance()->SetShadowsEnabled(false);
        Environment::GetInstance()->SetNumCascades(4);
        Environment::GetInstance()->GetProbeManager()->SetEnvMapEnabled(true);
        Environment::GetInstance()->GetProbeManager()->SetSphericalHarmonicsEnabled(true);
        Environment::GetInstance()->GetProbeManager()->SetVCTEnabled(true);

        GetRenderer()->GetPostProcessing()->AddFilter<SSAOFilter>("ssao", 5);
        //GetRenderer()->GetPostProcessing()->AddFilter<DepthOfFieldFilter>("depth of field", 50);
        //GetRenderer()->GetPostProcessing()->AddFilter<BloomFilter>("bloom", 80);
        //GetRenderer()->GetPostProcessing()->AddFilter<GammaCorrectionFilter>("gamma correction", 100);
        GetRenderer()->GetPostProcessing()->AddFilter<FXAAFilter>("fxaa", 9999);
        GetRenderer()->SetDeferred(true);

        AudioManager::GetInstance()->Initialize();

        Environment::GetInstance()->GetSun().SetDirection(Vector3(0.5).Normalize());

        auto gi_test_node = std::make_shared<Entity>("gi_test_node");
        gi_test_node->Move(Vector3(0, 5, 0));
        gi_test_node->AddControl(std::make_shared<GIProbeControl>(Vector3(0.0f), BoundingBox(Vector3(-25.0f), Vector3(25.0f))));
        GetScene()->AddChild(gi_test_node);


        GetCamera()->SetTranslation(Vector3(0, 0, 0));

        if (false) {
            auto dragger = asset_manager->LoadFromFile<Entity>("models/ogrexml/dragger_Body.mesh.xml");
            dragger->SetName("dragger");
            dragger->Move(Vector3(0, 3, 0));
            dragger->Scale(0.25);
            dragger->GetChild(0)->GetMaterial().diffuse_color = { 0.0f, 0.0f, 1.0f, 1.0f };
            dragger->GetControl<SkeletonControl>(0)->SetLoop(true);
            dragger->GetControl<SkeletonControl>(0)->PlayAnimation(0, 12.0);
            GetScene()->AddChild(dragger);
            dragger->UpdateTransform();
        }


        // Initialize particle system
        // InitParticleSystem();

        auto ui_text = std::make_shared<ui::UIText>("text_test", "Hyperion 0.1.0\n"
            "Press 1 to toggle shadows\n"
            "Press 2 to toggle deferred rendering\n"
            "Press 3 to toggle environment cubemapping\n"
            "Press 4 to toggle spherical harmonics mapping\n"
            "Press 5 to toggle voxel cone tracing\n");
        ui_text->SetLocalTranslation2D(Vector2(-1.0, 1.0));
        ui_text->SetLocalScale2D(Vector2(20));
        GetUI()->AddChild(ui_text);
        GetUIManager()->RegisterUIObject(ui_text);

        auto fps_counter = std::make_shared<ui::UIText>("fps_coutner", "- FPS");
        fps_counter->SetLocalTranslation2D(Vector2(0.8, 1.0));
        fps_counter->SetLocalScale2D(Vector2(30));
        GetUI()->AddChild(fps_counter);
        GetUIManager()->RegisterUIObject(fps_counter);

        m_selected_node_text = std::make_shared<ui::UIText>("selected_node_text", "No object selected");
        m_selected_node_text->SetLocalTranslation2D(Vector2(-1.0, -0.8));
        m_selected_node_text->SetLocalScale2D(Vector2(15));
        GetUI()->AddChild(m_selected_node_text);
        GetUIManager()->RegisterUIObject(m_selected_node_text);

        m_rotate_mode_btn = std::make_shared<ui::UIButton>("rotate_mode");
        m_rotate_mode_btn->SetLocalTranslation2D(Vector2(-1.0, -0.6));
        m_rotate_mode_btn->SetLocalScale2D(Vector2(15));
        GetUI()->AddChild(m_rotate_mode_btn);
        GetUIManager()->RegisterUIObject(m_rotate_mode_btn);

        auto cm = InitCubemap();

        GetScene()->AddControl(std::make_shared<SkydomeControl>(GetCamera()));

        bool write = false;
        bool read = true;

        if (!write && !read) {
            InitTestArea();
        } else {
            if (write) {
                InitTestArea();
                FileByteWriter fbw("models/scene.fbom");
                fbom::FBOMWriter writer;
                writer.Append(GetScene()->GetChild("model").get());
                auto res = writer.Emit(&fbw);
                fbw.Close();

                if (res != fbom::FBOMResult::FBOM_OK) {
                    throw std::runtime_error(std::string("FBOM Error: ") + res.message);
                }
            }

            if (read) {

                if (auto result = fbom::FBOMLoader().LoadFromFile("models/scene.fbom")) {
                    if (auto entity = std::dynamic_pointer_cast<Entity>(result.loadable)) {
                        for (size_t i = 0; i < entity->NumChildren(); i++) {
                            if (auto child = entity->GetChild(i)) {
                                if (auto ren = child->GetRenderable()) {
                                    ren->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties()));
                                }
                            }
                        }

                        GetScene()->AddChild(entity);
                        entity->GetChild("mesh0_SG")->AddControl(std::make_shared<EnvMapProbeControl>(Vector3(0.0f, 2.0f, 0.0f)));
                    }
                }
            }
        }

        for (int x = 0; x < 5; x++) {
            for (int z = 0; z < 5; z++) {
                Vector3 box_position = Vector3(((float(x) - 2.5) * 8), 3.0f, (float(z) - 2.5) * 8);
                auto box = asset_manager->LoadFromFile<Entity>("models/sphere_hq.obj", true);
                box->Scale(0.7f);

                for (size_t i = 0; i < box->NumChildren(); i++) {
                    Vector3 col = Vector3(
                        MathUtil::Random(0.4f, 1.80f),
                        MathUtil::Random(0.4f, 1.80f),
                        MathUtil::Random(0.4f, 1.80f)
                    ).Normalize();

                    box->GetChild(i)->GetMaterial().diffuse_color = Vector4(
                        1,//col.x,
                        1,//col.y,
                        1,//col.z,
                        1.0f
                    );

                    box->GetChild(0)->GetMaterial().SetTexture("DiffuseMap", asset_manager->LoadFromFile<Texture2D>("textures/steelplate/steelplate1_albedo.png"));
                    box->GetChild(0)->GetMaterial().SetTexture("ParallaxMap", asset_manager->LoadFromFile<Texture2D>("textures/steelplate/steelplate1_height.png"));
                    //box->GetChild(0)->GetMaterial().SetTexture("AoMap", asset_manager->LoadFromFile<Texture2D>("textures/steelplate/steelplate1_ao.png"));
                    box->GetChild(0)->GetMaterial().SetTexture("NormalMap", asset_manager->LoadFromFile<Texture2D>("textures/steelplate/steelplate1_normal-ogl.png"));
                    box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, 0.25f);
                    box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, 0.8f);
                    //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_METALNESS, x / 5.0f);
                    //box->GetChild(i)->GetMaterial().SetParameter(MATERIAL_PARAMETER_ROUGHNESS, z / 5.0f);
                }

                box->SetLocalTranslation(box_position);
                GetScene()->AddChild(box);
            }
        }


        /*int total_cascades = Environment::GetInstance()->NumCascades();
        for (int x = 0; x < 2; x++) {
            for (int z = 0; z < 2; z++) {
                auto ui_fbo_view = std::make_shared<ui::UIObject>("fbo_preview_" + std::to_string(x * 2 + z));
                ui_fbo_view->GetMaterial().SetTexture("ColorMap", Environment::GetInstance()->GetShadowMap(x * 2 + z));
                ui_fbo_view->SetLocalTranslation2D(Vector2(0.7 + (double(x) * 0.2), -0.4 + (double(z) * -0.3)));
                ui_fbo_view->SetLocalScale2D(Vector2(256));
                GetUI()->AddChild(ui_fbo_view);
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
        ui_crosshair->GetMaterial().SetTexture("ColorMap", asset_manager->LoadFromFile<Texture2D>("textures/crosshair.png"));
        ui_crosshair->SetLocalTranslation2D(Vector2(0));
        ui_crosshair->SetLocalScale2D(Vector2(128));
        GetUI()->AddChild(ui_crosshair);
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

            GetRenderer()->SetDeferred(!GetRenderer()->IsDeferred());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_3, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            ProbeManager::GetInstance()->SetEnvMapEnabled(!ProbeManager::GetInstance()->EnvMapEnabled());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_4, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            ProbeManager::GetInstance()->SetSphericalHarmonicsEnabled(!ProbeManager::GetInstance()->SphericalHarmonicsEnabled());
        }));

        GetInputManager()->RegisterKeyEvent(KEY_5, InputEvent([=](bool pressed) {
            if (!pressed) {
                return;
            }

            ProbeManager::GetInstance()->SetVCTEnabled(!ProbeManager::GetInstance()->VCTEnabled());
        }));

        /*auto house = asset_manager->LoadFromFile<Entity>("models/house.obj");
        GetScene()->AddChild(house);

        auto sd = asset_manager->LoadFromFile<Entity>("models/superdan/superdan.obj");
        sd->SetLocalTranslation(Vector3(0, 4, 0));
        GetScene()->AddChild(sd);*/

        /*std::shared_ptr<Loadable> result = fbom::FBOMLoader().LoadFromFile("./test.fbom");

        std::cout << "Result: " << typeid(*result.get()).name() << "\n";
        if (auto entity = std::dynamic_pointer_cast<Entity>(result)) {
            std::cout << "Loaded entity name: " << entity->GetName() << "\n";
            std::cout << "entity num children: " << entity->NumChildren() << "\n";
            entity->Scale(1.0f);
            for (size_t i = 0; i < entity->NumChildren(); i++) {
                std::cout << "child [" << i << "] renderable == " << intptr_t(entity->GetChild(i)->GetRenderable().get()) << "\n";
                if (entity->GetChild(i)->GetRenderable() == nullptr) {
                    continue;
                }
                entity->GetChild(i)->GetRenderable()->SetShader(shader);
            }
            GetScene()->AddChild(entity);
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

                AssertThrow(m_hit_to_entity.find(m_ray_hit.GetHashCode().Value()) != m_hit_to_entity.end());

                m_selected_node = m_hit_to_entity[m_ray_hit.GetHashCode().Value()];
                m_selected_node->AddControl(std::make_shared<BoundingBoxControl>());


                std::stringstream ss;
                ss << "Selected object: ";
                ss << m_selected_node->GetName();
                ss << " ";
                ss << m_selected_node->GetGlobalTranslation();
                ss << " " << m_selected_node->GetAABB();
                
                m_selected_node_text->SetText(ss.str());

                /*auto cube = GetScene()->GetChild("cube");
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

                    AssertThrow(intersected_with != nullptr);

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

        AudioManager::GetInstance()->SetListenerPosition(GetCamera()->GetTranslation());
        AudioManager::GetInstance()->SetListenerOrientation(GetCamera()->GetDirection(), GetCamera()->GetUpVector());

        PhysicsManager::GetInstance()->RunPhysics(dt);
    }

    void OnRender()
    {
        if (Environment::GetInstance()->ShadowsEnabled()) {
            Vector3 shadow_dir = Environment::GetInstance()->GetSun().GetDirection() * -1;
            // shadow_dir.SetY(-1.0f);
            shadows->SetOrigin(GetCamera()->GetTranslation());
            shadows->SetLightDirection(shadow_dir);
            shadows->Render(GetRenderer());
        }

        // TODO: ProbeControl on top node
        /*if (Environment::GetInstance()->ProbeEnabled()) {
            //Environment::GetInstance()->GetProbeRenderer()->SetOrigin(Vector3(GetCamera()->GetTranslation()));
            Environment::GetInstance()->GetProbeRenderer()->Render(GetRenderer(), GetCamera());

            if (!Environment::GetInstance()->GetGlobalCubemap()) {
                Environment::GetInstance()->SetGlobalCubemap(
                    Environment::GetInstance()->GetProbeRenderer()->GetColorTexture()
                );
            }
        }*/
    }
};

int main()
{
    std::string base_path = HYP_ROOT_DIR;

    json::SourceFile sf(base_path + "/config.json", 200);
    sf.operator>>(std::string("{ \"hello\": [1,2,3,4], \"Test nested\": { \"key\": \"value\", \"other\": 5.345435e-2 } }"));
    json::SourceStream ss(&sf);
    json::TokenStream ts(json::TokenStreamInfo("./config.json"));
    json::State state;
    auto json_lex = json::Lexer(ss, &ts, &state);
    json_lex.Analyze();
    auto json_parser = json::Parser(&ts, &state);
    auto json_object = json_parser.Parse();
    std::cout << "json: " << json_object.ToString() << "\n";



    AssetManager::GetInstance()->SetRootDir(base_path + "/res/");


    SystemSDL system;
    SystemWindow *window = SystemSDL::CreateSystemWindow("Hyperion Engine", 1024, 768);
    system.SetCurrentWindow(window);

    SystemEvent event;

    VkRenderer renderer(system, "Hyperion Vulkan Test", "HyperionEngine");

    renderer.Initialize(true);
    renderer.CreateSurface();
    renderer.InitializeRendererDevice();
    renderer.InitializeSwapchain();

    RendererDevice *device = renderer.GetRendererDevice();

    RendererShader shader;
    shader.AttachShader(device, SPIRVObject{ SPIRVObject::Type::Vertex, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/vert.spv").Read() });
    shader.AttachShader(device, SPIRVObject{ SPIRVObject::Type::Fragment, FileByteReader(AssetManager::GetInstance()->GetRootDir() + "vkshaders/frag.spv").Read() });
    shader.CreateProgram("main");

    renderer.InitializePipeline(&shader);

    bool running = true;
    while (running) {
        while (SystemSDL::PollEvent(&event)) {
            switch (event.GetType()) {
                case SystemEventType::EVENT_SHUTDOWN:
                    running = false;
                    break;
                case SystemEventType::EVENT_KEYDOWN:
                    DebugLog(LogType::Info, "Keydown captured!");
                    break;
                default:
                    break;
            }
        }
        renderer.DrawFrame();
    }

    shader.Destroy();
    renderer.Destroy();
    delete window;

    return  0;


    // timing test
    /*{ // fbom
        using namespace std;
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
    
        // Call the function, here sort()
        std::shared_ptr<Loadable> result;
        for (int i = 0; i < 100; i++) {
            result = fbom::FBOMLoader().LoadFromFile("./test.fbom");
        }
    
        // Get ending timepoint
        auto stop = high_resolution_clock::now();
    
        // Get duration. Substart timepoints to 
        // get durarion. To cast it to proper unit
        // use duration cast method
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();

        std::cout << "FBOM time: " << duration << "\n";
    }
    // timing test
    { // obj
        using namespace std;
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
    
        // Call the function, here sort()
        std::shared_ptr<Loadable> result;
        for (int i = 0; i < 100; i++) {
            result = asset_manager->LoadFromFile<Entity>("models/sphere_hq.obj", false);
        }
    
        // Get ending timepoint
        auto stop = high_resolution_clock::now();
    
        // Get duration. Substart timepoints to 
        // get durarion. To cast it to proper unit
        // use duration cast method
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(stop - start).count();

        std::cout << "OBJ time: " << duration << "\n";
    }*/


    // std::shared_ptr<Entity> my_entity = std::make_shared<Entity>("FOO BAR");
    // my_entity->AddControl(std::make_shared<NoiseTerrainControl>(nullptr, 12345));

    // auto my_entity = asset_manager->LoadFromFile<Entity>("models/sphere_hq.obj", true);
    // my_entity->Scale(Vector3(0.2f));
    // my_entity->Move(Vector3(0, 2, 0));

    // FileByteWriter fbw("test.fbom");
    // fbom::FBOMWriter writer;
    // writer.Append(my_entity.get());
    // auto res = writer.Emit(&fbw);
    // fbw.Close();

    // if (res != fbom::FBOMResult::FBOM_OK) {
    //     throw std::runtime_error(std::string("FBOM Error: ") + res.message);
    // }

    // return 0;

    /*std::shared_ptr<Loadable> result = fbom::FBOMLoader().LoadFromFile("./test.fbom");

    if (auto entity = std::dynamic_pointer_cast<Entity>(result)) {
        std::cout << "Loaded entity name: " << entity->GetName() << "\n";
    }

    return 0;*/

    CoreEngine *engine = new GlfwEngine();
    CoreEngine::SetInstance(engine);

    auto *game = new SceneEditor(RenderWindow(1480, 1200, "Hyperion Demo"));

    engine->InitializeGame(game);

    delete game;
    delete engine;

    return 0;
}
