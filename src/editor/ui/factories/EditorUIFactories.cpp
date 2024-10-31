#include <editor/ui/EditorUI.hpp>

#include <asset/Assets.hpp>

#include <scene/Node.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <ui/UIText.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIStage.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIDataSource.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/threading/Task.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypProperty.hpp>

#include <core/logging/Logger.hpp>

#include <core/Defines.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

class HypDataUIDataSourceElementFactory : public UIDataSourceElementFactory<HypData, HypDataUIDataSourceElementFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const HypData &value) const
    {
        const HypClass *hyp_class = GetClass(value.GetTypeID());
        AssertThrowMsg(hyp_class != nullptr, "No HypClass registered for TypeID %u", value.GetTypeID().Value());

        if (value.IsNull()) {
            RC<UIText> empty_value_text = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            empty_value_text->SetText("Object is null");

            return empty_value_text;
        }

        RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        HashMap<String, HypProperty *> properties_by_name;

        for (auto it = hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).Begin(); it != hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY).End(); ++it) {
            if (HypProperty *property = dynamic_cast<HypProperty *>(&*it)) {
                if (!property->GetAttribute("editor")) {
                    continue;
                }

                if (!property->CanGet() || !property->CanSet()) {
                    continue;
                }

                properties_by_name[property->name.LookupString()] = property;
            } else {
                HYP_UNREACHABLE();
            }
        }

        for (auto &it : properties_by_name) {
            RC<UIGridRow> row = grid->AddRow();

            RC<UIGridColumn> column = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            HypData getter_result = it.second->Get(value);
            
            IUIDataSourceElementFactory *factory = GetEditorUIDataSourceElementFactory(getter_result.GetTypeID());
            
            if (!factory) {
                HYP_LOG(Editor, LogLevel::WARNING, "No factory registered for TypeID {} when creating UI element for attribute \"{}\"", getter_result.GetTypeID().Value(), it.first);

                continue;
            }

            RC<UIObject> element = factory->CreateUIObject(stage, getter_result);
            AssertThrow(element != nullptr);

            HYP_LOG(Editor, LogLevel::DEBUG, "Element for attribute \"{}\": {}\tsize: {}", it.first, GetClass(element.GetTypeID())->GetName(), element->GetActualSize());
            
            panel->AddChildUIObject(element);

            column->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const HypData &value) const
    {
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(HypData, HypDataUIDataSourceElementFactory);



template <int StringType>
class StringUIDataSourceElementFactory : public UIDataSourceElementFactory<containers::detail::String<StringType>, StringUIDataSourceElementFactory<StringType>>
{
public:
    RC<UIObject> Create(UIStage *stage, const containers::detail::String<StringType> &value) const
    {
        RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
        textbox->SetText(value.ToUTF8());
        
        return textbox;
    }

    void Update(UIObject *ui_object, const containers::detail::String<StringType> &value) const
    {
        ui_object->SetText(value.ToUTF8());
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::ANSI>, StringUIDataSourceElementFactory<StringType::ANSI>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::UTF8>, StringUIDataSourceElementFactory<StringType::UTF8>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::UTF16>, StringUIDataSourceElementFactory<StringType::UTF16>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::UTF32>, StringUIDataSourceElementFactory<StringType::UTF32>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::detail::String<StringType::WIDE_CHAR>, StringUIDataSourceElementFactory<StringType::WIDE_CHAR>);

class Vec3fUIDataSourceElementFactory : public UIDataSourceElementFactory<Vec3f, Vec3fUIDataSourceElementFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const Vec3f &value) const
    {
        RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UIGridRow> row = grid->AddRow();

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("Vec3fPanel_X"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });
            
            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(NAME("Vec3fPanel_X_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.x));
            panel->AddChildUIObject(textbox); 

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("Vec3fPanel_Y"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(NAME("Vec3fPanel_Y_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.y));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("Vec3fPanel_Z"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(NAME("Vec3fPanel_Z_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.z));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const Vec3f &value) const
    {
        ui_object->FindChildUIObject(NAME("Vec3fPanel_X_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.x));

        ui_object->FindChildUIObject(NAME("Vec3fPanel_Y_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.y));

        ui_object->FindChildUIObject(NAME("Vec3fPanel_Z_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.z));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Vec3f, Vec3fUIDataSourceElementFactory);


class Uint32UIDataSourceElementFactory : public UIDataSourceElementFactory<uint32, Uint32UIDataSourceElementFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const uint32 &value) const
    {
        RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UIGridRow> row = grid->AddRow();

        {
            RC<UIGridColumn> col = row->AddColumn();
            
            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(NAME("Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value));

            col->AddChildUIObject(textbox);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const uint32 &value) const
    {
        ui_object->FindChildUIObject(NAME("Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(uint32, Uint32UIDataSourceElementFactory);


class QuaternionUIDataSourceElementFactory : public UIDataSourceElementFactory<Quaternion, QuaternionUIDataSourceElementFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const Quaternion &value) const
    {
        RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        RC<UIGridRow> row = grid->AddRow();

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Roll"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });
            
            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Roll_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Roll()));
            panel->AddChildUIObject(textbox); 

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Pitch"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Pitch_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Pitch()));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            RC<UIGridColumn> col = row->AddColumn();

            RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Yaw"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            RC<UITextbox> textbox = stage->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Yaw_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Yaw()));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject *ui_object, const Quaternion &value) const
    {
        ui_object->FindChildUIObject(NAME("QuaternionPanel_Roll_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.Roll()));

        ui_object->FindChildUIObject(NAME("QuaternionPanel_Pitch_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.Pitch()));

        ui_object->FindChildUIObject(NAME("QuaternionPanel_Yaw_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.Yaw()));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Quaternion, QuaternionUIDataSourceElementFactory);

class TransformUIDataSourceElementFactory : public UIDataSourceElementFactory<Transform, TransformUIDataSourceElementFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const Transform &value) const
    {
        const HypClass *hyp_class = GetClass<Transform>();

        RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        {
            RC<UIGridRow> translation_header_row = grid->AddRow();
            RC<UIGridColumn> translation_header_column = translation_header_row->AddColumn();

            RC<UIText> translation_header = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            translation_header->SetText("Translation");
            translation_header_column->AddChildUIObject(translation_header);

            RC<UIGridRow> translation_value_row = grid->AddRow();
            RC<UIGridColumn> translation_value_column = translation_value_row->AddColumn();
            
            if (IUIDataSourceElementFactory *factory = GetEditorUIDataSourceElementFactory<Vec3f>()) {
                RC<UIObject> translation_element = factory->CreateUIObject(stage, HypData(value.GetTranslation()));
                translation_value_column->AddChildUIObject(translation_element);
            }
        }

        {
            RC<UIGridRow> rotation_header_row = grid->AddRow();
            RC<UIGridColumn> rotation_header_column = rotation_header_row->AddColumn();

            RC<UIText> rotation_header = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            rotation_header->SetText("Rotation");
            rotation_header_column->AddChildUIObject(rotation_header);

            RC<UIGridRow> rotation_value_row = grid->AddRow();
            RC<UIGridColumn> rotation_value_column = rotation_value_row->AddColumn();
            
            if (IUIDataSourceElementFactory *factory = GetEditorUIDataSourceElementFactory<Quaternion>()) {
                RC<UIObject> rotation_element = factory->CreateUIObject(stage, HypData(value.GetRotation()));
                rotation_value_column->AddChildUIObject(rotation_element);
            }
        }

        {
            RC<UIGridRow> scale_header_row = grid->AddRow();
            RC<UIGridColumn> scale_header_column = scale_header_row->AddColumn();

            RC<UIText> scale_header = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            scale_header->SetText("Scale");
            scale_header_column->AddChildUIObject(scale_header);

            RC<UIGridRow> scale_value_row = grid->AddRow();
            RC<UIGridColumn> scale_value_column = scale_value_row->AddColumn();
            
            if (IUIDataSourceElementFactory *factory = GetEditorUIDataSourceElementFactory<Vec3f>()) {
                RC<UIObject> scale_element = factory->CreateUIObject(stage, HypData(value.GetScale()));
                scale_value_column->AddChildUIObject(scale_element);
            }
        }

        return grid;
    }

    void Update(UIObject *ui_object, const Transform &value) const
    {
        HYP_NOT_IMPLEMENTED_VOID();

        // ui_object->FindChildUIObject(NAME("TranslationValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetTranslation()));

        // ui_object->FindChildUIObject(NAME("RotationValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetRotation()));

        // ui_object->FindChildUIObject(NAME("ScaleValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetScale()));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Transform, TransformUIDataSourceElementFactory);

class EditorWeakNodeFactory : public UIDataSourceElementFactory<Weak<Node>, EditorWeakNodeFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const Weak<Node> &value) const
    {
        String node_name = "Invalid";
        UUID node_uuid = UUID::Invalid();

        if (RC<Node> node_rc = value.Lock()) {
            node_name = node_rc->GetName();
            node_uuid = node_rc->GetUUID();
        } else {
            node_uuid = UUID();
        }

        RC<UIText> text = stage->CreateUIObject<UIText>(CreateNameFromDynamicString(ANSIString("Node_") + node_uuid.ToString()), Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        text->SetText(node_name);
        
        return text;
    }

    void Update(UIObject *ui_object, const Weak<Node> &value) const
    {
        static const String invalid_node_name = "<Invalid>";

        if (UIText *text = dynamic_cast<UIText *>(ui_object)) {
            if (RC<Node> node_rc = value.Lock()) {
                text->SetText(node_rc->GetName());
            } else {
                text->SetText(invalid_node_name);
            }
        }
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Weak<Node>, EditorWeakNodeFactory);

class EntityUIDataSourceElementFactory : public UIDataSourceElementFactory<ID<Entity>, EntityUIDataSourceElementFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const ID<Entity> &entity) const
    {
        const EditorNodePropertyRef *context = GetContext<EditorNodePropertyRef>();
        AssertThrow(context != nullptr);

        if (!entity.IsValid()) {
            RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            RC<UIGridRow> row = grid->AddRow();
            RC<UIGridColumn> column = row->AddColumn();

            RC<UIButton> add_entity_button = stage->CreateUIObject<UIButton>(NAME("Add_Entity_Button"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            add_entity_button->SetText("Add Entity");
            add_entity_button->OnClick.Bind([node_weak = context->node](...) -> UIEventHandlerResult
            {
                HYP_LOG(Editor, LogLevel::DEBUG, "Add Entity clicked");

                if (RC<Node> node_rc = node_weak.Lock()) {
                    Scene *scene = node_rc->GetScene();

                    if (!scene) {
                        HYP_LOG(Editor, LogLevel::ERR, "GetScene() returned null for Node with name \"{}\", cannot add Entity", node_rc->GetName());

                        return UIEventHandlerResult::ERR;
                    }

                    const ID<Entity> entity = scene->GetEntityManager()->AddEntity();
                    node_rc->SetEntity(entity);

                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                HYP_LOG(Editor, LogLevel::ERR, "Cannot add Entity to Node, Node reference could not be obtained");

                return UIEventHandlerResult::ERR;
            }).Detach();

            column->AddChildUIObject(add_entity_button);

            return grid;
        }

        EntityManager *entity_manager = EntityManager::GetEntityToEntityManagerMap().GetEntityManager(entity);

        if (!entity_manager) {
            HYP_LOG(Editor, LogLevel::ERR, "No EntityManager found for entity {}", entity.Value());
            
            return nullptr;
        }

        auto CreateComponentsGrid = [&]() -> RC<UIObject>
        {
            Optional<const TypeMap<ComponentID> &> all_components = entity_manager->GetAllComponents(entity);

            if (!all_components.HasValue()) {
                HYP_LOG(Editor, LogLevel::ERR, "No component map found for Entity #{}", entity.Value());

                return nullptr;
            }

            RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            for (const auto &it : *all_components) {
                const TypeID component_type_id = it.first;

                const ComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

                if (!component_interface) {
                    HYP_LOG(Editor, LogLevel::ERR, "No ComponentInterface registered for component with TypeID {}", component_type_id.Value());

                    continue;
                }

                if (component_interface->GetClass() && !component_interface->GetClass()->GetAttribute("editor", true)) {
                    // Skip components that are not meant to be edited in the editor
                    continue;
                }

                IUIDataSourceElementFactory *factory = GetEditorUIDataSourceElementFactory(component_type_id);

                if (!factory) {
                    HYP_LOG(Editor, LogLevel::ERR, "No editor UI component factory registered for component of type \"{}\"", component_interface->GetTypeName());

                    continue;
                }

                ComponentContainerBase *component_container = entity_manager->TryGetContainer(component_type_id);
                AssertThrow(component_container != nullptr);

                HypData component_hyp_data;

                if (!component_container->TryGetComponent(it.second, component_hyp_data)) {
                    HYP_LOG(Editor, LogLevel::ERR, "Failed to get component of type \"{}\" with ID {} for Entity #{}", component_interface->GetTypeName(), it.second, entity.Value());

                    continue;
                }

                RC<UIGridRow> header_row = grid->AddRow();
                RC<UIGridColumn> header_column = header_row->AddColumn();

                RC<UIText> component_header = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

                Optional<String> component_header_text_opt;
                Optional<String> component_description_opt;

                if (component_interface->GetClass()) {
                    if (const HypClassAttributeValue &attr = component_interface->GetClass()->GetAttribute("label")) {
                        component_header_text_opt = attr.GetString();
                    }

                    if (const HypClassAttributeValue &attr = component_interface->GetClass()->GetAttribute("description")) {
                        component_description_opt = attr.GetString();
                    }
                }
            
                if (!component_header_text_opt.HasValue()) {
                    component_header_text_opt = component_interface->GetTypeName();
                }

                component_header->SetText(*component_header_text_opt);
                component_header->SetTextSize(12);
                header_column->AddChildUIObject(component_header);

                if (component_description_opt.HasValue()) {
                    RC<UIGridRow> description_row = grid->AddRow();
                    RC<UIGridColumn> description_column = description_row->AddColumn();

                    RC<UIText> component_description = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                    component_description->SetTextSize(10);
                    component_description->SetText(*component_description_opt);

                    description_column->AddChildUIObject(component_description);
                }

                RC<UIGridRow> content_row = grid->AddRow();
                RC<UIGridColumn> content_column = content_row->AddColumn();

                RC<UIPanel> component_content = stage->CreateUIObject<UIPanel>(Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

                RC<UIObject> element = factory->CreateUIObject(stage, component_hyp_data);
                AssertThrow(element != nullptr);

                component_content->AddChildUIObject(element);

                content_column->AddChildUIObject(component_content);
            }

            return grid;
        };

        RC<UIGrid> components_grid_container = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
        
        RC<UIGridRow> components_grid_container_header_row = components_grid_container->AddRow();
        RC<UIGridColumn> components_grid_container_header_column = components_grid_container_header_row->AddColumn();

        RC<UIText> components_grid_container_header_text = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
        components_grid_container_header_text->SetText("Components");
        components_grid_container_header_column->AddChildUIObject(components_grid_container_header_text);

        RC<UIButton> add_component_button = stage->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        add_component_button->SetText("Add Component");
        add_component_button->OnClick.Bind([stage_weak = stage->WeakRefCountedPtrFromThis()](...)
        {
            HYP_LOG(Editor, LogLevel::DEBUG, "Add Component clicked");

            if (RC<UIStage> stage = stage_weak.Lock().Cast<UIStage>()) {
                auto loaded_ui_asset = AssetManager::GetInstance()->Load<RC<UIObject>>("ui/dialog/Component.Add.ui.xml");
                
                if (loaded_ui_asset.IsOK()) {
                    auto loaded_ui = loaded_ui_asset.Result();

                    if (RC<UIObject> add_component_window = loaded_ui->FindChildUIObject("Add_Component_Window")) {
                        stage->AddChildUIObject(add_component_window);
                
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }

                HYP_LOG(Editor, LogLevel::ERR, "Failed to load add component ui dialog! Error: {}", loaded_ui_asset.result.message);

                return UIEventHandlerResult::ERR;

                // RC<UIWindow> window = stage->CreateUIObject<UIWindow>(Vec2i { 0, 0 }, UIObjectSize(Vec2i { 250, 500 }));
                // window->SetOriginAlignment(UIObjectAlignment::CENTER);
                // window->SetParentAlignment(UIObjectAlignment::CENTER);
                // window->SetText("Add Component");

                // RC<UIGrid> window_content_grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                // RC<UIGridRow> window_content_grid_row = window_content_grid->AddRow();
                // RC<UIGridColumn> window_content_grid_column = window_content_grid_row->AddColumn();

                // RC<UIGrid> window_footer_grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

                // RC<UIGridRow> window_footer_grid_row = window_footer_grid->AddRow();

                // RC<UIButton> add_button = stage->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                // add_button->SetText("Add");
                // add_button->OnClick.Bind([window_weak = window.ToWeak()](...)
                // {
                //     // @TODO

                //     return UIEventHandlerResult::ERR;
                // }).Detach();
                // window_footer_grid_row->AddColumn()->AddChildUIObject(add_button);

                // RC<UIButton> cancel_button = stage->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                // cancel_button->SetText("Cancel");
                // cancel_button->OnClick.Bind([window_weak = window.ToWeak()](...)
                // {
                //     if (RC<UIWindow> window = window_weak.Lock()) {
                //         if (window->RemoveFromParent()) {
                //             return UIEventHandlerResult::STOP_BUBBLING;
                //         }
                //     }

                //     return UIEventHandlerResult::ERR;
                // }).Detach();

                // window_footer_grid_row->AddColumn()->AddChildUIObject(cancel_button);

                // window_content_grid->AddRow()->AddColumn()->AddChildUIObject(window_footer_grid);

                // window->AddChildUIObject(window_content_grid);

                // window_content_grid_row->SetSize(UIObjectSize({ 100, UIObjectSize::PERCENT }, { 100, UIObjectSize::FILL }));

                // stage->AddChildUIObject(window);
            }

            return UIEventHandlerResult::ERR;
        }).Detach();

        components_grid_container_header_column->AddChildUIObject(add_component_button);

        RC<UIGridRow> components_grid_container_content_row = components_grid_container->AddRow();
        RC<UIGridColumn> components_grid_container_content_column = components_grid_container_content_row->AddColumn();

        if (entity_manager->GetOwnerThreadMask() & Threads::CurrentThreadID()) {
            components_grid_container_content_column->AddChildUIObject(CreateComponentsGrid());
        } else {
            HYP_NAMED_SCOPE("Awaiting async component UI element creation");

            Task<RC<UIObject>> task;

            entity_manager->PushCommand([&CreateComponentsGrid, executor = task.Initialize()](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                executor->Fulfill(CreateComponentsGrid());
            });

            components_grid_container_content_column->AddChildUIObject(task.Await());
        }

        return components_grid_container;
    }

    void Update(UIObject *ui_object, const ID<Entity> &entity) const
    {
        // @TODO

        HYP_NOT_IMPLEMENTED_VOID();
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(ID<Entity>, EntityUIDataSourceElementFactory);

class EditorNodePropertyFactory : public UIDataSourceElementFactory<EditorNodePropertyRef, EditorNodePropertyFactory>
{
public:
    RC<UIObject> Create(UIStage *stage, const EditorNodePropertyRef &value) const
    {
        RC<Node> node_rc = value.node.Lock();
        
        if (!node_rc) {
            HYP_LOG(Editor, LogLevel::ERR, "Node reference is invalid, cannot create UI element for property \"{}\"", value.title);

            return nullptr;
        }

        IUIDataSourceElementFactory *factory = GetEditorUIDataSourceElementFactory(value.property->GetTypeID());

        if (!factory) {
            HYP_LOG(Editor, LogLevel::ERR, "No factory registered for TypeID {} when creating UI element for property \"{}\"", value.property->GetTypeID().Value(), value.title);

            return nullptr;
        }

        // Create panel
        RC<UIPanel> panel = stage->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        {
            RC<UIGrid> grid = stage->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            RC<UIGridRow> header_row = grid->AddRow();
            RC<UIGridColumn> header_column = header_row->AddColumn();

            RC<UIText> component_header = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

            component_header->SetText(value.title);
            component_header->SetTextSize(12);
            header_column->AddChildUIObject(component_header);

            if (value.description.HasValue()) {
                RC<UIGridRow> description_row = grid->AddRow();
                RC<UIGridColumn> description_column = description_row->AddColumn();

                RC<UIText> component_description = stage->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                component_description->SetTextSize(10);
                component_description->SetText(*value.description);

                description_column->AddChildUIObject(component_description);
            }

            RC<UIGridRow> content_row = grid->AddRow();
            RC<UIGridColumn> content_column = content_row->AddColumn();

            panel->AddChildUIObject(grid);
        }

        {
            RC<UIPanel> content = stage->CreateUIObject<UIPanel>(NAME("PropertyPanel_Content"), Vec2i { 0, 25 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            if (RC<UIObject> element = factory->CreateUIObject(stage, value.property->Get(HypData(node_rc)), ConstAnyRef(&value))) {
                content->AddChildUIObject(element);
            }

            panel->AddChildUIObject(content);
        }

        return panel;
    }

    void Update(UIObject *ui_object, const EditorNodePropertyRef &value) const
    {
        RC<Node> node_rc = value.node.Lock();
        AssertThrow(node_rc != nullptr);

        IUIDataSourceElementFactory *factory = GetEditorUIDataSourceElementFactory(value.property->GetTypeID());
        AssertThrow(factory != nullptr);

        RC<UIPanel> content = ui_object->FindChildUIObject(WeakName("PropertyPanel_Content")).Cast<UIPanel>();
        AssertThrow(content != nullptr);
        
        content->RemoveAllChildUIObjects();

        RC<UIObject> element = factory->CreateUIObject(ui_object->GetStage(), value.property->Get(HypData(node_rc)), ConstAnyRef(&value));
        AssertThrow(element != nullptr);

        content->AddChildUIObject(element);
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(EditorNodePropertyRef, EditorNodePropertyFactory);

} // namespace hyperion