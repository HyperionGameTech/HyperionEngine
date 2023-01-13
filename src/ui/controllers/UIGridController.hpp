//
// Created by emd22 on 13/12/22.
//

#ifndef HYPERION_V2_UI_GRID_CONTROLLER_HPP
#define HYPERION_V2_UI_GRID_CONTROLLER_HPP

#include <math/Vector2.hpp>
#include <ui/controllers/UIController.hpp>

namespace hyperion::v2 {

class UIGridController : public UIController
{
public:
public:
    using Cell = Vector2;

    UIGridController(const String &name, bool receives_update = true);
    virtual ~UIGridController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnTransformUpdate(const Transform &transform) override;

    Vector2 GetGridCell(Extent2D position)
    {
        return m_cell_size * Vector2(position.x, position.y);
    }

    Vector4 GetGridRect(Extent2D position, Extent2D dimensions)
    {
        const Vector4 rect {
            GetGridCell(position),
            GetGridCell(position + dimensions)
        };
        return rect;
    }

    void SetGridDivisions(Extent2D divisions)
    {
        m_grid_divisions = divisions;
    }

protected:
    virtual bool CreateScriptedMethods() override;

    Vector2 m_cell_size;
    Extent2D m_grid_divisions;
};

} // namespace hyperion::v2

#endif //HYPERION_UIGRID_HPP
