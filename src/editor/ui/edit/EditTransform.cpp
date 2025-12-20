#include <EditorPch.hpp>

#include <editor/ui/edit/EditTransform.hpp>
#include <editor/ui/EditorUI.hpp>

#include <core/reflection/Property.hpp>
#include <scene/Node.hpp>

#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>
#include <ui/UIDataSource.hpp>

#include <EditTransform.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditTransform::EditTransform()
    : EditorPropertyPanelBase()
{
}

EditTransform::~EditTransform() = default;

void EditTransform::Build_Impl(const BoxedValue& boxed, const Property* property)
{
    HYP_NAMED_SCOPE("EditTransform::Build");

    Assert(boxed.IsValid());

    const Handle<Node>& node = boxed.Get<Handle<Node>>();
    Assert(node != nullptr);

    Assert(property != nullptr);
    Assert(property->CanGet());

    BoxedValue resultData = property->Get(boxed);
    Assert(resultData.IsValid());

    Transform transform = resultData.Get<Transform>();
    m_currentValue = std::move(resultData);

    OnValueChange
        .Bind([nodeWeak = node.ToWeak(), property](const BoxedValue& value) -> UIEventHandlerResult
            {
                Handle<Node> node = nodeWeak.Lock();
                if (!node || !property->CanSet())
                {
                    return UIEventHandlerResult::ERR;
                }

                NodeUnlockTransformScope scope(*node);

                BoxedValue targetData(node.ToRef());
                property->Set(targetData, value);

                return UIEventHandlerResult::OK;
            })
        .Detach();

    Handle<UIGrid> grid = CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    AddChildUIObject(grid);

    {
        Handle<UIGridRow> translationHeaderRow = grid->AddRow();
        Handle<UIGridColumn> translationHeaderColumn = translationHeaderRow->AddColumn();

        Handle<UIText> translationHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        translationHeader->SetText("Translation");
        translationHeaderColumn->AddChildUIObject(translationHeader);

        Handle<UIGridRow> translationValueRow = grid->AddRow();
        Handle<UIGridColumn> translationValueColumn = translationValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
        {
            Handle<UIObject> translationElement = factory->CreateUIObject(this, BoxedValue(transform.GetTranslation()), {});

            m_delegateHandlers.Add(translationElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const BoxedValue& value) -> UIEventHandlerResult
                {
                    Handle<EditTransform> strongThis = weakThis.Lock();
                    if (!strongThis)
                    {
                        return UIEventHandlerResult::OK;
                    }

                    Transform currentValue = GetCurrentValue().Get<Transform>();
                    currentValue.SetTranslation(value.Get<Vec3f>());
                    SetCurrentValue(BoxedValue(currentValue));

                    return UIEventHandlerResult::OK;
                }));

            translationValueColumn->AddChildUIObject(translationElement);
        }
    }

    {
        Handle<UIGridRow> rotationHeaderRow = grid->AddRow();
        Handle<UIGridColumn> rotationHeaderColumn = rotationHeaderRow->AddColumn();

        Handle<UIText> rotationHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        rotationHeader->SetText("Rotation");
        rotationHeaderColumn->AddChildUIObject(rotationHeader);

        Handle<UIGridRow> rotationValueRow = grid->AddRow();
        Handle<UIGridColumn> rotationValueColumn = rotationValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Quaternion>())
        {
            Handle<UIObject> rotationElement = factory->CreateUIObject(this, BoxedValue(transform.GetRotation()), {});

            m_delegateHandlers.Add(rotationElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const BoxedValue& value) -> UIEventHandlerResult
                {
                    Handle<EditTransform> strongThis = weakThis.Lock();
                    if (!strongThis)
                    {
                        return UIEventHandlerResult::OK;
                    }

                    Transform currentValue = GetCurrentValue().Get<Transform>();
                    currentValue.SetRotation(value.Get<Quaternion>());
                    SetCurrentValue(BoxedValue(currentValue));

                    return UIEventHandlerResult::OK;
                }));

            rotationValueColumn->AddChildUIObject(rotationElement);
        }
    }

    {
        Handle<UIGridRow> scaleHeaderRow = grid->AddRow();
        Handle<UIGridColumn> scaleHeaderColumn = scaleHeaderRow->AddColumn();

        Handle<UIText> scaleHeader = CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        scaleHeader->SetText("Scale");
        scaleHeaderColumn->AddChildUIObject(scaleHeader);

        Handle<UIGridRow> scaleValueRow = grid->AddRow();
        Handle<UIGridColumn> scaleValueColumn = scaleValueRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
        {
            Handle<UIObject> scaleElement = factory->CreateUIObject(this, BoxedValue(transform.GetScale()), {});

            m_delegateHandlers.Add(scaleElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const BoxedValue& value) -> UIEventHandlerResult
                {
                    Handle<EditTransform> strongThis = weakThis.Lock();
                    if (!strongThis)
                    {
                        return UIEventHandlerResult::OK;
                    }

                    Transform currentValue = GetCurrentValue().Get<Transform>();
                    currentValue.SetScale(value.Get<Vec3f>());
                    SetCurrentValue(BoxedValue(currentValue));

                    return UIEventHandlerResult::OK;
                }));

            scaleValueColumn->AddChildUIObject(scaleElement);
        }
    }
}

} // namespace hyperion
