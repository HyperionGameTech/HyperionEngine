#include <HyperionPch.hpp>

#include <editor/ui/edit/EditBoundingBox.hpp>
#include <editor/ui/EditorUI.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/reflection/Property.hpp>

#include <core/math/BoundingBox.hpp>

#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>
#include <ui/UIDataSource.hpp>

#include <core/logging/Logger.hpp>

#include <EditBoundingBox.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditBoundingBox::EditBoundingBox()
    : EditorPropertyPanelBase()
{
}

EditBoundingBox::~EditBoundingBox() = default;

void EditBoundingBox::Build_Impl(const HypData& hypData, const Property* property)
{
    HYP_NAMED_SCOPE("EditBoundingBox::Build");

    Assert(hypData.IsValid());

    const Handle<ObjectBase>& target = hypData.Get<Handle<ObjectBase>>();
    Assert(target != nullptr);

    Assert(property != nullptr);
    Assert(property->CanGet());

    HypData resultData = property->Get(hypData);
    Assert(resultData.IsValid());

    BoundingBox boundingBox = resultData.Get<BoundingBox>();
    m_currentValue = std::move(resultData);

    OnValueChange
        .Bind([targetWeak = MakeWeakRef(target), property](const HypData& value) -> UIEventHandlerResult
            {
                Handle<ObjectBase> target = targetWeak.Lock();
                if (!target || !property->CanSet())
                {
                    return UIEventHandlerResult::ERR;
                }

                HypData targetData(target.ToRef());
                property->Set(targetData, value);

                return UIEventHandlerResult::OK;
            })
        .Detach();

    Handle<UIGrid> grid = CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    AddChildUIObject(grid);

    {
        Handle<UIGridRow> minHeaderRow = grid->AddRow();
        Handle<UIGridColumn> minHeaderColumn = minHeaderRow->AddColumn();

        Handle<UIText> minHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        minHeader->SetText("Min");
        minHeaderColumn->AddChildUIObject(minHeader);

        Handle<UIGridRow> minValueRow = grid->AddRow();
        Handle<UIGridColumn> minValueColumn = minValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
        {
            Handle<UIObject> minElement = factory->CreateUIObject(this, HypData(boundingBox.min), {});

            m_delegateHandlers.Add(minElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const HypData& value) -> UIEventHandlerResult
                {
                    Handle<EditBoundingBox> strongThis = weakThis.Lock();
                    if (!strongThis)
                    {
                        return UIEventHandlerResult::OK;
                    }

                    BoundingBox currentValue = GetCurrentValue().Get<BoundingBox>();
                    currentValue.min = value.Get<Vec3f>();
                    SetCurrentValue(HypData(currentValue));

                    return UIEventHandlerResult::OK;
                }));

            minValueColumn->AddChildUIObject(minElement);
        }
    }

    {
        Handle<UIGridRow> maxHeaderRow = grid->AddRow();
        Handle<UIGridColumn> maxHeaderColumn = maxHeaderRow->AddColumn();

        Handle<UIText> maxHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        maxHeader->SetText("Max");
        maxHeaderColumn->AddChildUIObject(maxHeader);

        Handle<UIGridRow> maxValueRow = grid->AddRow();
        Handle<UIGridColumn> maxValueColumn = maxValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
        {
            Handle<UIObject> maxElement = factory->CreateUIObject(this, HypData(boundingBox.max), {});

            m_delegateHandlers.Add(maxElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const HypData& value) -> UIEventHandlerResult
                {
                    Handle<EditBoundingBox> strongThis = weakThis.Lock();
                    if (!strongThis)
                    {
                        return UIEventHandlerResult::OK;
                    }

                    BoundingBox currentValue = GetCurrentValue().Get<BoundingBox>();
                    currentValue.max = value.Get<Vec3f>();
                    SetCurrentValue(HypData(currentValue));

                    return UIEventHandlerResult::OK;
                }));

            maxValueColumn->AddChildUIObject(maxElement);
        }
    }
}

} // namespace hyperion
