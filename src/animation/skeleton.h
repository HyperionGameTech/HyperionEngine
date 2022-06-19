#ifndef HYPERION_V2_SKELETON_H
#define HYPERION_V2_SKELETON_H

#include <rendering/base.h>
#include <rendering/shader.h>
#include "../animation/animation.h"

namespace hyperion::v2 {

class Engine;
class Bone;

class Skeleton : public EngineComponentBase<STUB_CLASS(Skeleton)> {
public:
    Skeleton();
    Skeleton(std::unique_ptr<Bone> &&root_bone);
    Skeleton(const Skeleton &other) = delete;
    Skeleton &operator=(const Skeleton &other) = delete;
    ~Skeleton();

    ShaderDataState GetShaderDataState() const { return m_shader_data_state; }
    void SetShaderDataState(ShaderDataState state) { m_shader_data_state = state; }

    /*! \brief Look up a bone with the given name/tag. If no root bone was set,
     * or the bone could not be found, nullptr is returned. Otherwise, the resulting bone
     * pointer is returned.
     */
    Bone *FindBone(const char *name) const;

    /*! \brief Get the root Bone of this skeleton, which all nested Bones fall under.
     * If no root bone was set on this Skeleton, nullptr is returned
     * @returns The root bone of this skeleton, or nullptr
     */
    Bone *GetRootBone() const { return m_root_bone.get(); }

    void SetRootBone(std::unique_ptr<Bone> &&root_bone);

    /*! \brief Find a Bone on this skeleton with the given tag. If no bone could
     * be found, nullptr is returned. If no root bone has been set, nullptr is also returned.
     * @param tag The string tag of the Bone to find
     * @returns A pointer to the found Bone, or nullptr
     */
    //Bone *FindBone(const char *tag) const;

    auto &GetAnimations()                                     { return m_animations; }
    const auto &GetAnimations() const                         { return m_animations; }
    size_t NumAnimations() const                              { return m_animations.size(); }
    void AddAnimation(std::unique_ptr<Animation> &&animation);

    Animation *GetAnimation(size_t index) const               { return m_animations[index].get(); }
    Animation *FindAnimation(const std::string &name, UInt *out_index) const;
    
    void Init(Engine *engine);
    void EnqueueRenderUpdates(Engine *engine) const;

private:
    size_t NumBones() const;
    
    std::unique_ptr<Bone> m_root_bone;
    std::vector<std::unique_ptr<Animation>> m_animations;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif