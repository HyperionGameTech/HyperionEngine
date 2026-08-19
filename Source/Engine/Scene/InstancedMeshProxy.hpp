/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Node.hpp>

namespace Hyperion {

/*! \brief Represents an instance of a mesh (shared Mesh, Material) with a unique transform from its parent */
HYP_CLASS()
class ENGINE_API InstancedMeshProxy : public Node
{
    HYP_OBJECT_BODY(InstancedMeshProxy);

public:
    InstancedMeshProxy();

    InstancedMeshProxy(const InstancedMeshProxy& other) = delete;
    InstancedMeshProxy& operator=(const InstancedMeshProxy& other) = delete;

    ~InstancedMeshProxy();

    Mat4f prevTransformMatrix;

protected:
    virtual void OnAttachedToNode(Node* node) override;
    virtual void OnDetachedFromNode(Node* node) override;

    virtual void OnTransformUpdated() override;
};

} // namespace Hyperion
