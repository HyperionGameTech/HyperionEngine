#include <EditorPch.hpp>

#include <editor/ui/edit/EditName.hpp>
#include <editor/ui/EditorUI.hpp>

#include <Core/profiling/ProfileScope.hpp>

#include <Core/reflection/Property.hpp>

#include <scene/Node.hpp>

#include <ui/UIGrid.hpp>
#include <ui/UIText.hpp>
#include <ui/UIDataSource.hpp>

#include <EditName.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditName::EditName()
    : EditorPropertyPanelBase()
{
}

EditName::~EditName() = default;

void EditName::Build_Impl(const BoxedValue& boxed, const Property* property)
{
    HYP_NAMED_SCOPE("EditName::Build");

    Assert(boxed.IsValid());

    const Handle<Node>& node = boxed.Get<Handle<Node>>();
    Assert(node != nullptr);

    Assert(property != nullptr);
    Assert(property->CanGet());

    BoxedValue resultData = property->Get(boxed);
    Assert(resultData.IsValid());

    Name nameValue = resultData.Get<Name>();
    m_currentValue = std::move(resultData);

    OnValueChange
        .Bind([nodeWeak = node.ToWeak(), property](const BoxedValue& value) -> UIEventHandlerResult
            {
                Handle<Node> node = nodeWeak.Lock();
                if (!node || !property->CanSet())
                {
                    return UIEventHandlerResult::ERR;
                }

                BoxedValue targetData(node.ToRef());
                property->Set(targetData, value);

                return UIEventHandlerResult::OK;
            })
        .Detach();

    Handle<UIGrid> grid = CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
    AddChildUIObject(grid);

    {
        Handle<UIGridRow> nameRow = grid->AddRow();
        Handle<UIGridColumn> nameValueColumn = nameRow->AddColumn();

        if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<String>())
        {
            Handle<UIObject> nameElement = factory->CreateUIObject(this, BoxedValue(nameValue.ToString()), {});

            m_delegateHandlers.Add(nameElement->OnValueChange.Bind([this, weakThis = WeakHandleFromThis()](const BoxedValue& value) -> UIEventHandlerResult
                {
                    Handle<EditName> strongThis = weakThis.Lock();
                    if (!strongThis)
                    {
                        return UIEventHandlerResult::OK;
                    }

                    String str = value.Get<String>();
                    Name newName = CreateNameFromDynamicString(str);

                    SetCurrentValue(BoxedValue(newName));

                    return UIEventHandlerResult::OK;
                }));

            nameValueColumn->AddChildUIObject(nameElement);
        }
    }
}

} // namespace Hyperion
