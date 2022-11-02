#ifndef HYPERION_V2_SKELETON_H
#define HYPERION_V2_SKELETON_H

#include <core/Base.hpp>
#include <rendering/ShaderDataState.hpp>
#include <core/lib/DynArray.hpp>
#include <scene/animation/Animation.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class Engine;
class Bone;

class Skeleton
    : public EngineComponentBase<STUB_CLASS(Skeleton)>,
      public RenderResource
{
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
    Bone *FindBone(const String &name);

    /*! \brief Look up a bone with the given name/tag. If no root bone was set,
     * or the bone could not be found, nullptr is returned. Otherwise, the resulting bone
     * pointer is returned.
     */
    const Bone *FindBone(const String &name) const
        { return const_cast<Skeleton *>(this)->FindBone(name); }

    /*! \brief Get the root Bone of this skeleton, which all nested Bones fall under.
     * If no root bone was set on this Skeleton, nullptr is returned
     * @returns The root bone of this skeleton, or nullptr
     */
    Bone *GetRootBone() const { return m_root_bone.get(); }

    void SetRootBone(std::unique_ptr<Bone> &&root_bone);

    auto &GetAnimations() { return m_animations; }
    const auto &GetAnimations() const { return m_animations; }
    SizeType NumAnimations() const { return m_animations.Size(); }
    void AddAnimation(std::unique_ptr<Animation> &&animation);

    Animation *GetAnimation(SizeType index) const { return m_animations[index].get(); }
    Animation *FindAnimation(const String &name, UInt *out_index) const;
    
    void Init(Engine *engine);
    void EnqueueRenderUpdates() const;

private:
    SizeType NumBones() const;
    
    std::unique_ptr<Bone> m_root_bone;
    Array<std::unique_ptr<Animation>> m_animations;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif