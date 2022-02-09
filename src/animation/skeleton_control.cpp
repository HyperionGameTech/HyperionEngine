#include "skeleton_control.h"

namespace hyperion {
SkeletonControl::SkeletonControl(std::shared_ptr<Shader> skinning_shader)
    : EntityControl(fbom::FBOMObjectType("SKELETON_CONTROL"), 60.0),
      skinning_shader(skinning_shader),
      play_state(STOPPED),
      loop_mode(PLAY_ONCE),
      current_anim(nullptr),
      time(0.0)
{
}

void SkeletonControl::SetLoop(bool loop)
{
    loop_mode = loop ? LOOP : PLAY_ONCE;
}

Bone *SkeletonControl::GetBone(const std::string &name)
{
    for (auto &&bone : bones) {
        if (bone->GetName() == name) {
            return bone;
        }
    }
    return nullptr;
}

std::shared_ptr<Animation> SkeletonControl::GetAnimation(size_t index)
{
    return animations[index];
}

std::shared_ptr<Animation> SkeletonControl::GetAnimation(const std::string &name)
{
    for (auto &&anim : animations) {
        if (anim->GetName() == name) {
            return anim;
        }
    }

    return nullptr;
}

void SkeletonControl::AddAnimation(std::shared_ptr<Animation> anim)
{
    animations.push_back(anim);
}

size_t SkeletonControl::NumAnimations() const
{
    return animations.size();
}

void SkeletonControl::PlayAnimation(int index, double speed)
{
    if (index != -1) {
        current_anim = animations.at(index).get();
        play_speed = speed;
        play_state = PLAYING;
    } else {
        StopAnimation();
    }
}

void SkeletonControl::PlayAnimation(const std::string &name, double speed)
{
    int index = -1;
    for (int i = 0; i < animations.size(); i++) {
        if (animations[i]->GetName() == name) {
            index = i;
        }
    }
    if (index == -1) {
        throw "Animation not found";
    }
    PlayAnimation(index, speed);
}

void SkeletonControl::PauseAnimation()
{
    play_state = PAUSED;
}

void SkeletonControl::StopAnimation()
{
    play_speed = 1.0;
    play_state = STOPPED;
}

void SkeletonControl::FindBones(Entity *top)
{
    auto *bone = dynamic_cast<Bone*>(top);
    if (bone) {
        bone_names.push_back("Bone[" + std::to_string(bones.size()) + "]");
        bones.push_back(bone);
    }
    for (size_t i = 0; i < top->NumChildren(); i++) {
        FindBones(top->GetChild(i).get());
    }
}

void SkeletonControl::OnAdded()
{
    FindBones(parent);
}

void SkeletonControl::OnRemoved()
{
    bone_names.clear();
    bones.clear();
}

void SkeletonControl::OnUpdate(double dt)
{
    const double step = 0.01;
    if (play_state == PLAYING && current_anim != nullptr) {
        time += step * play_speed;
        if (time > current_anim->GetLength()) {
            time = 0.0;
            if (loop_mode == PLAY_ONCE) {
                StopAnimation();
            }
        }
        current_anim->ApplyBlended(time, 0.5);
    }

    for (size_t i = 0; i < bones.size(); i++) {
        skinning_shader->SetUniform(bone_names[i], bones[i]->GetBoneMatrix());
    }
}

std::shared_ptr<EntityControl> SkeletonControl::CloneImpl()
{
    std::shared_ptr<SkeletonControl> clone = std::make_shared<SkeletonControl>(skinning_shader);

    // clone->bone_names = bone_names;

    clone->animations.reserve(animations.size());

    for (const auto &anim : animations) {
        if (anim == nullptr) {
            continue;
        }

        clone->animations.push_back(std::make_shared<Animation>(*anim));
    }

    clone->play_speed = play_speed;
    clone->loop_mode = loop_mode;

    return clone;
}
}
