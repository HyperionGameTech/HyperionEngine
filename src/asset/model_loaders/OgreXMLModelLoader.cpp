/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/model_loaders/OgreXMLModelLoader.hpp>

#include <scene/World.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/AnimationComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

#include <rendering/RenderMesh.hpp>
#include <rendering/RenderMaterial.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

#include <util/xml/SAXParser.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

using OgreXMLModel = OgreXMLModelLoader::OgreXMLModel;
using BoneAssignment = OgreXMLModelLoader::OgreXMLModel::BoneAssignment;
using SubMesh = OgreXMLModelLoader::OgreXMLModel::SubMesh;

class OgreXMLSAXHandler : public xml::SAXHandler
{
public:
    OgreXMLSAXHandler(LoaderState *state, OgreXMLModel &object)
        : m_model(object)
    {
    }

    SubMesh &LastSubMesh()
    {
        if (m_model.submeshes.Empty()) {
            m_model.submeshes.PushBack({ });
        }

        return m_model.submeshes.Back();
    }

    void AddBoneAssignment(uint32 vertex_index, BoneAssignment &&bone_assignment)
    {
        m_model.bone_assignments[vertex_index].PushBack(std::move(bone_assignment));
    }

    virtual void Begin(const String &name, const xml::AttributeMap &attributes) override
    {
        if (name == "position") {
            if (!attributes.Contains("x") || !attributes.Contains("y") || !attributes.Contains("z")) {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("x"));
            const auto y = StringUtil::Parse<float>(attributes.At("y"));
            const auto z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.positions.PushBack(Vec3f(x, y, z));
        } else if (name == "normal") {
            if (!attributes.Contains("x") || !attributes.Contains("y") || !attributes.Contains("z")) {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("x"));
            const auto y = StringUtil::Parse<float>(attributes.At("y"));
            const auto z = StringUtil::Parse<float>(attributes.At("z"));

            m_model.normals.PushBack(Vec3f(x, y, z));
        } else if (name == "texcoord") {
            if (!attributes.Contains("u") || !attributes.Contains("v")) {
                return;
            }

            const auto x = StringUtil::Parse<float>(attributes.At("u"));
            const auto y = StringUtil::Parse<float>(attributes.At("v"));

            m_model.texcoords.PushBack(Vec2f(x, y));
        } else if (name == "face") {
            if (attributes.Size() != 3) {
                HYP_LOG(Assets, Warning, "Ogre XML parser: `face` tag expected to have 3 attributes.");
            }
            
            FlatMap<String, uint32> face_elements;
            face_elements.Reserve(attributes.Size());

            for (const Pair<String, String> &it : attributes) {
                face_elements.Insert({ it.first, StringUtil::Parse<uint32>(it.second) });
            }

            for (const Pair<String, uint32> &it : face_elements) {
                LastSubMesh().indices.PushBack(it.second);
            }
        } else if (name == "skeletonlink") {
            m_model.skeleton_name = attributes.At("name");
        } else if (name == "vertexboneassignment") {
            const auto vertex_index = StringUtil::Parse<uint32>(attributes.At("vertexindex"));
            const auto bone_index = StringUtil::Parse<uint32>(attributes.At("boneindex"));
            const auto bone_weight = StringUtil::Parse<float>(attributes.At("weight"));

            AddBoneAssignment(vertex_index, {bone_index, bone_weight});
        } else if (name == "submesh") {
            String name = String("submesh_") + String::ToString(m_model.submeshes.Size());

            if (auto name_it = attributes.Find("material"); name_it != attributes.End()) {
                name = name_it->second;
            }

            m_model.submeshes.PushBack({
                name,
                Array<uint32> { }
            });
        } else if (name == "vertex") {
            /* no-op */
        }  else {
            HYP_LOG(Assets, Warning, "Ogre XML parser: No handler for '{}' tag", name);
        }
    }

    virtual void End(const String &name) override {}
    virtual void Characters(const String &value) override {}
    virtual void Comment(const String &comment) override {}

private:
    OgreXMLModel &m_model;
};

void BuildVertices(OgreXMLModel &model)
{
    const bool has_normals = !model.normals.Empty(),
        has_texcoords = !model.texcoords.Empty();

    Array<Vertex> vertices;
    vertices.Resize(model.positions.Size());

    for (uint32 i = 0; i < vertices.Size(); i++) {
        Vec3f position;
        Vec3f normal;
        Vec2f texcoord;

        if (i < model.positions.Size()) {
            position = model.positions[i];
        } else {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Vertex index ({}) out of bounds ({})", i, model.positions.Size());

            continue;
        }

        if (has_normals) {
            if (i < model.normals.Size()) {
                normal = model.normals[i];
            } else {
                HYP_LOG(Assets, Warning, "Ogre XML parser: Normal index ({}) out of bounds ({})", i, model.normals.Size());
            }
        }

        if (has_texcoords) {
            if (i < model.texcoords.Size()) {
                texcoord = model.texcoords[i];
            } else {
                HYP_LOG(Assets, Warning, "Ogre XML parser: Texcoord index ({}) out of bounds ({})", i, model.texcoords.Size());
            }
        }

        vertices[i] = Vertex(position, texcoord, normal);

        const auto bone_it = model.bone_assignments.Find(i);

        if (bone_it != model.bone_assignments.end()) {
            auto &bone_assignments = bone_it->second;

            for (SizeType j = 0; j < bone_assignments.Size(); j++) {
                if (j == 4) {
                    HYP_LOG(Assets, Warning, "Ogre XML parser: Attempt to add more than 4 bone assignments");

                    break;
                }

                vertices[i].AddBoneIndex(bone_assignments[j].index);
                vertices[i].AddBoneWeight(bone_assignments[j].weight);
            }
        }
    }

    model.vertices = std::move(vertices);
}

LoadedAsset OgreXMLModelLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    OgreXMLModel model;
    model.filepath = state.filepath;

    OgreXMLSAXHandler handler(&state, model);

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        return { { LoaderResult::Status::ERR, sax_result.message } };
    }
    
    BuildVertices(model);

    NodeProxy top(MakeRefCountedPtr<Node>());

    Handle<Skeleton> skeleton;

    if (!model.skeleton_name.Empty()) {
        const String skeleton_path((StringUtil::BasePath(model.filepath.Data()).c_str() + String("/") + model.skeleton_name + ".xml").Data());

        auto skeleton_asset = state.asset_manager->Load<Skeleton>(skeleton_path);

        if (skeleton_asset.IsOK()) {
            skeleton = skeleton_asset.Result();
        } else {
            HYP_LOG(Assets, Warning, "Ogre XML parser: Could not load skeleton at {}", skeleton_path);
        }
    }

    for (SubMesh &sub_mesh : model.submeshes) {
        if (sub_mesh.indices.Empty()) {
            HYP_LOG(Assets, Info, "Ogre XML parser: Skipping submesh with empty indices");

            continue;
        }

        Handle<Scene> scene = g_engine->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadID());

        const Handle<Entity> entity = scene->GetEntityManager()->AddEntity();

        scene->GetEntityManager()->AddComponent<TransformComponent>(
            entity,
            TransformComponent { }
        );

        scene->GetEntityManager()->AddComponent<VisibilityStateComponent>(
            entity,
            VisibilityStateComponent { }
        );

        Handle<Mesh> mesh = CreateObject<Mesh>(
            model.vertices,
            sub_mesh.indices,
            Topology::TRIANGLES
        );

        // if (model.normals.Empty()) {
            mesh->CalculateNormals();
        // }

        // mesh->CalculateTangents();
        InitObject(mesh);

        ShaderProperties shader_properties(mesh->GetVertexAttributes());

        Handle<Material> material = CreateObject<Material>(CreateNameFromDynamicString(ANSIString(sub_mesh.name.Data())));
        material->SetShader(ShaderManager::GetInstance()->GetOrCreate(NAME("Forward"), shader_properties));
        InitObject(material);

        scene->GetEntityManager()->AddComponent<MeshComponent>(
            entity,
            MeshComponent {
                mesh,
                material,
                skeleton
            }
        );

        scene->GetEntityManager()->AddComponent<BoundingBoxComponent>(
            entity,
            BoundingBoxComponent {
                mesh->GetAABB()
            }
        );

        if (skeleton.IsValid()) {
            InitObject(skeleton);

            scene->GetEntityManager()->AddComponent<AnimationComponent>(
                entity,
                AnimationComponent {
                    {
                        .animation_index    = 0,
                        .status             = AnimationPlaybackStatus::PLAYING,
                        .loop_mode          = AnimationLoopMode::REPEAT,
                        .speed              = 1.0f,
                        .current_time       = 0.0f
                    }
                }
            );
        }
        
        NodeProxy node(MakeRefCountedPtr<Node>());
        node->SetName(sub_mesh.name);
        node->SetEntity(entity);

        top->AddChild(std::move(node));
    }

    return { { LoaderResult::Status::OK }, top };
}

} // namespace hyperion
