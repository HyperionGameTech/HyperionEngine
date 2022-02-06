#ifndef SKELETON_H
#define SKELETON_H

#include "../asset/loadable.h"
#include "animation.h"
#include "../rendering/renderer.h"

#include <string>
#include <vector>

namespace hyperion {
class Skeleton : public Loadable {
public:
    Skeleton(const std::string &name = "");
    virtual ~Skeleton() = default;

    std::shared_ptr<Animation> GetAnimation(const std::string &name);
    std::shared_ptr<Animation> GetAnimation(size_t index);
    void AddAnimation(std::shared_ptr<Animation>);

    std::shared_ptr<Bone> GetBone(const std::string &name);
    std::shared_ptr<Bone> GetBone(size_t index);
    void AddBone(std::shared_ptr<Bone>);

    std::vector<std::shared_ptr<Animation>> animations;
    std::vector<std::shared_ptr<Bone>> bones;

    virtual std::shared_ptr<Loadable> Clone() override;

private:
    std::string m_name;
};
}

#endif
