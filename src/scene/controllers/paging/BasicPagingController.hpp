#ifndef HYPERION_V2_BASIC_PAGING_CONTROLLER_H
#define HYPERION_V2_BASIC_PAGING_CONTROLLER_H

#include <scene/controllers/PagingController.hpp>

namespace hyperion::v2 {

class BasicPagingController : public PagingController
{
public:
    BasicPagingController(
        Extent3D patch_size,
        const Vector3 &scale,
        Float max_distance
    );
    virtual ~BasicPagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;

protected:
    virtual void OnPatchAdded(Patch *patch) override;
    virtual void OnPatchRemoved(Patch *patch) override;
};

} // namespace hyperion::v2

#endif