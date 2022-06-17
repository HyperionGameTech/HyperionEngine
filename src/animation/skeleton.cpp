#include "skeleton.h"
#include "bone.h"
#include "../engine.h"

namespace hyperion::v2 {

Skeleton::Skeleton()
    : EngineComponentBase(),
      m_root_bone(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
}

Skeleton::Skeleton(std::unique_ptr<Bone> &&root_bone)
    : EngineComponentBase(),
      m_root_bone(std::move(root_bone))
{
    if (m_root_bone != nullptr) {
        m_root_bone->SetSkeleton(this);
    }
}

Skeleton::~Skeleton()
{
    Teardown();
}

void Skeleton::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SKELETONS, [this](Engine *engine) {
        EnqueueRenderUpdates(engine);
        
        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SKELETONS, [this](Engine *engine) {
            HYP_FLUSH_RENDER_QUEUE(engine);
            
            SetReady(false);
        }), engine);
    }));
}

void Skeleton::EnqueueRenderUpdates(Engine *engine) const
{
    if (!m_shader_data_state.IsDirty()) {
        return;
    }

    const size_t num_bones = MathUtil::Min(SkeletonShaderData::max_bones, NumBones());

    if (num_bones != 0) {
        SkeletonShaderData &shader_data = engine->shader_globals->skeletons.Get(m_id.value - 1); /* TODO: is this fully thread safe? */

        shader_data.bones[0] = m_root_bone->GetBoneMatrix();

        for (size_t i = 1; i < num_bones; i++) {
            if (auto *descendent = m_root_bone->GetDescendents()[i - 1]) {
                if (descendent->GetType() != Node::Type::BONE) {
                    continue;
                }

                shader_data.bones[i] = static_cast<Bone *>(descendent)->GetBoneMatrix();  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            }
        }

        engine->shader_globals->skeletons.Set(m_id.value - 1, shader_data);
    }
    
    m_shader_data_state = ShaderDataState::CLEAN;
}

Bone *Skeleton::FindBone(const char *name) const
{
    if (m_root_bone == nullptr) {
        return nullptr;
    }

    if (!std::strcmp(m_root_bone->GetName(), name)) {
        return m_root_bone.get();
    }

    for (Node *node : m_root_bone->GetDescendents()) {
        if (node == nullptr || node->GetType() != Node::Type::BONE) {
            continue;
        }

        auto *bone = static_cast<Bone *>(node);  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        if (!std::strcmp(bone->GetName(), name)) {
            return bone;
        }
    }

    return nullptr;
}

void Skeleton::SetRootBone(std::unique_ptr<Bone> &&root_bone)
{
    m_root_bone = std::move(root_bone);

    if (m_root_bone != nullptr) {
        m_root_bone->SetSkeleton(this);
    }
}

size_t Skeleton::NumBones() const
{
    if (m_root_bone == nullptr) {
        return 0;
    }

    return 1 + m_root_bone->GetDescendents().size();
}

void Skeleton::AddAnimation(std::unique_ptr<Animation> &&animation)
{
    if (animation == nullptr) {
        return;
    }

    for (auto &track : animation->GetTracks()) {
        track.bone = nullptr;

        if (track.bone_name.empty()) {
            continue;
        }

        track.bone = FindBone(track.bone_name.c_str());

        if (track.bone == nullptr) {
            DebugLog(
                LogType::Warn,
                "Skeleton could not find bone with name \"%s\"\n",
                track.bone_name.c_str()
            );
        }
    }

    m_animations.push_back(std::move(animation));
}

Animation *Skeleton::FindAnimation(const std::string &name, size_t *out_index) const
{
    const auto it = std::find_if(
        m_animations.begin(),
        m_animations.end(),
        [&name](const auto &item) {
            return item->GetName() == name;
        }
    );

    if (it == m_animations.end()) {
        return nullptr;
    }

    if (out_index != nullptr) {
        *out_index = std::distance(m_animations.begin(), it);
    }

    return it->get();
}

} // namespace hyperion::v2
