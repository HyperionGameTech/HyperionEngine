#include <editor/ui/EditorUI.hpp>
#include <editor/ui/EditorPropertyPanel.hpp>
#include <editor/EditorAction.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/Node.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/components/ScriptComponent.hpp>

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

class HypDataUIElementFactory : public UIElementFactory<HypData>
{
public:
    Handle<UIObject> Create(UIObject* parent, const HypData& value) const
    {
        const HypClass* hypClass = GetClass(value.GetTypeId());
        Assert(hypClass != nullptr, "No HypClass registered for TypeId %u", value.GetTypeId().Value());

        if (value.IsNull())
        {
            Handle<UIText> emptyValueText = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            emptyValueText->SetText("Object is null");

            return emptyValueText;
        }

        Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        HashMap<String, HypProperty*> propertiesByName;

        for (auto it = hypClass->GetMembers(HypMemberType::TYPE_PROPERTY).Begin(); it != hypClass->GetMembers(HypMemberType::TYPE_PROPERTY).End(); ++it)
        {
            if (HypProperty* property = dynamic_cast<HypProperty*>(&*it))
            {
                if (!property->GetAttribute("editor"))
                {
                    continue;
                }

                if (!property->CanGet())
                {
                    continue;
                }

                propertiesByName[property->GetName().LookupString()] = property;
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }

        for (auto& it : propertiesByName)
        {
            Handle<UIGridRow> row = grid->AddRow();

            Handle<UIGridColumn> column = row->AddColumn();

            Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            HypData getterResult = it.second->Get(value);

            Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory(getterResult.GetTypeId());

            if (!factory)
            {
                HYP_LOG(Editor, Warning, "No factory registered for TypeId {} when creating UI element for attribute \"{}\"", getterResult.GetTypeId().Value(), it.first);

                continue;
            }

            Handle<UIObject> element = factory->CreateUIObject(parent, getterResult, {});
            Assert(element != nullptr);
            panel->AddChildUIObject(element);

            column->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject* uiObject, const HypData& value) const
    {
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(HypData, HypDataUIElementFactory);

template <int StringType>
class StringUIElementFactory : public UIElementFactory<containers::String<StringType>>
{
public:
    Handle<UIObject> Create(UIObject* parent, const containers::String<StringType>& value) const
    {
        Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
        textbox->SetText(value.ToUTF8());

        return textbox;
    }

    void Update(UIObject* uiObject, const containers::String<StringType>& value) const
    {
        uiObject->SetText(value.ToUTF8());
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(containers::String<StringType::ANSI>, StringUIElementFactory<StringType::ANSI>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::String<StringType::UTF8>, StringUIElementFactory<StringType::UTF8>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::String<StringType::UTF16>, StringUIElementFactory<StringType::UTF16>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::String<StringType::UTF32>, StringUIElementFactory<StringType::UTF32>);
HYP_DEFINE_UI_ELEMENT_FACTORY(containers::String<StringType::WIDE_CHAR>, StringUIElementFactory<StringType::WIDE_CHAR>);

class Vec3fUIElementFactory : public UIElementFactory<Vec3f>
{
public:
    Handle<UIObject> Create(UIObject* parent, const Vec3f& value) const
    {
        Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        Handle<UIGridRow> row = grid->AddRow();

        {
            Handle<UIGridColumn> col = row->AddColumn();

            Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("Vec3fPanel_X"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Vec3fPanel_X_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.x));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            Handle<UIGridColumn> col = row->AddColumn();

            Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("Vec3fPanel_Y"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Vec3fPanel_Y_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.y));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            Handle<UIGridColumn> col = row->AddColumn();

            Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("Vec3fPanel_Z"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Vec3fPanel_Z_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.z));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject* uiObject, const Vec3f& value) const
    {
        uiObject->FindChildUIObject(NAME("Vec3fPanel_X_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.x));

        uiObject->FindChildUIObject(NAME("Vec3fPanel_Y_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.y));

        uiObject->FindChildUIObject(NAME("Vec3fPanel_Z_Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value.z));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Vec3f, Vec3fUIElementFactory);

class Uint32UIElementFactory : public UIElementFactory<uint32>
{
public:
    Handle<UIObject> Create(UIObject* parent, const uint32& value) const
    {
        Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        Handle<UIGridRow> row = grid->AddRow();

        {
            Handle<UIGridColumn> col = row->AddColumn();

            Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value));

            col->AddChildUIObject(textbox);
        }

        return grid;
    }

    void Update(UIObject* uiObject, const uint32& value) const
    {
        uiObject->FindChildUIObject(NAME("Value"))
            .Cast<UITextbox>()
            ->SetText(HYP_FORMAT("{}", value));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(uint32, Uint32UIElementFactory);

class QuaternionUIElementFactory : public UIElementFactory<Quaternion>
{
public:
    Handle<UIObject> Create(UIObject* parent, const Quaternion& value) const
    {
        Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        Handle<UIGridRow> row = grid->AddRow();

        {
            Handle<UIGridColumn> col = row->AddColumn();

            Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Roll"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Roll_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Roll()));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            Handle<UIGridColumn> col = row->AddColumn();

            Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Pitch"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Pitch_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Pitch()));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        {
            Handle<UIGridColumn> col = row->AddColumn();

            Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(NAME("QuaternionPanel_Yaw"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            panel->SetPadding({ 1, 1 });

            Handle<UITextbox> textbox = parent->CreateUIObject<UITextbox>(NAME("QuaternionPanel_Yaw_Value"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 20, UIObjectSize::PIXEL }));
            textbox->SetText(HYP_FORMAT("{}", value.Yaw()));
            panel->AddChildUIObject(textbox);

            col->AddChildUIObject(panel);
        }

        return grid;
    }

    void Update(UIObject* uiObject, const Quaternion& value) const
    {
        ObjCast<UITextbox>(uiObject->FindChildUIObject(NAME("QuaternionPanel_Roll_Value")))
            ->SetText(HYP_FORMAT("{}", MathUtil::RadToDeg(value.Roll())));

        ObjCast<UITextbox>(uiObject->FindChildUIObject(NAME("QuaternionPanel_Pitch_Value")))
            ->SetText(HYP_FORMAT("{}", MathUtil::RadToDeg(value.Pitch())));

        ObjCast<UITextbox>(uiObject->FindChildUIObject(NAME("QuaternionPanel_Yaw_Value")))
            ->SetText(HYP_FORMAT("{}", MathUtil::RadToDeg(value.Yaw())));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Quaternion, QuaternionUIElementFactory);

class TransformUIElementFactory : public UIElementFactory<Transform>
{
public:
    Handle<UIObject> Create(UIObject* parent, const Transform& value) const
    {
        const HypClass* hypClass = GetClass<Transform>();

        Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        {
            Handle<UIGridRow> translationHeaderRow = grid->AddRow();
            Handle<UIGridColumn> translationHeaderColumn = translationHeaderRow->AddColumn();

            Handle<UIText> translationHeader = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            translationHeader->SetText("Translation");
            translationHeaderColumn->AddChildUIObject(translationHeader);

            Handle<UIGridRow> translationValueRow = grid->AddRow();
            Handle<UIGridColumn> translationValueColumn = translationValueRow->AddColumn();

            if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
            {
                Handle<UIObject> translationElement = factory->CreateUIObject(parent, HypData(value.GetTranslation()), {});
                translationValueColumn->AddChildUIObject(translationElement);
            }
        }

        {
            Handle<UIGridRow> rotationHeaderRow = grid->AddRow();
            Handle<UIGridColumn> rotationHeaderColumn = rotationHeaderRow->AddColumn();

            Handle<UIText> rotationHeader = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            rotationHeader->SetText("Rotation");
            rotationHeaderColumn->AddChildUIObject(rotationHeader);

            Handle<UIGridRow> rotationValueRow = grid->AddRow();
            Handle<UIGridColumn> rotationValueColumn = rotationValueRow->AddColumn();

            if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Quaternion>())
            {
                Handle<UIObject> rotationElement = factory->CreateUIObject(parent, HypData(value.GetRotation()), {});
                rotationValueColumn->AddChildUIObject(rotationElement);
            }
        }

        {
            Handle<UIGridRow> scaleHeaderRow = grid->AddRow();
            Handle<UIGridColumn> scaleHeaderColumn = scaleHeaderRow->AddColumn();

            Handle<UIText> scaleHeader = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
            scaleHeader->SetText("Scale");
            scaleHeaderColumn->AddChildUIObject(scaleHeader);

            Handle<UIGridRow> scaleValueRow = grid->AddRow();
            Handle<UIGridColumn> scaleValueColumn = scaleValueRow->AddColumn();

            if (Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory<Vec3f>())
            {
                Handle<UIObject> scaleElement = factory->CreateUIObject(parent, HypData(value.GetScale()), {});
                scaleValueColumn->AddChildUIObject(scaleElement);
            }
        }

        return grid;
    }

    void Update(UIObject* uiObject, const Transform& value) const
    {
        HYP_NOT_IMPLEMENTED_VOID();

        // uiObject->FindChildUIObject(NAME("TranslationValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetTranslation()));

        // uiObject->FindChildUIObject(NAME("RotationValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetRotation()));

        // uiObject->FindChildUIObject(NAME("ScaleValue"))
        //     .Cast<UITextbox>()
        //     ->SetText(HYP_FORMAT("{}", value.GetScale()));
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Transform, TransformUIElementFactory);

class EditorWeakNodeFactory : public UIElementFactory<WeakHandle<Node>>
{
    static String GetNodeName(Node* node)
    {
        String nodeName = "Invalid";

        if (node->IsRoot() && node->GetScene() != nullptr)
        {
            nodeName = HYP_FORMAT("{} Scene Root", node->GetScene()->GetName());
        }
        else
        {
            nodeName = node->GetName().LookupString();
        }

        return nodeName;
    }

public:
    Handle<UIObject> Create(UIObject* parent, const WeakHandle<Node>& value) const
    {
        String nodeName = "Invalid";
        UUID nodeUuid = UUID::Invalid();

        if (Handle<Node> node = value.Lock())
        {
            nodeName = GetNodeName(node.Get());
            nodeUuid = node->GetUUID();
        }
        else
        {
            nodeUuid = UUID();
        }

        Handle<UIText> text = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        text->SetText(nodeName);

        return text;
    }

    void Update(UIObject* uiObject, const WeakHandle<Node>& value) const
    {
        static const String invalidNodeName = "<Invalid>";

        if (UIText* text = dynamic_cast<UIText*>(uiObject))
        {
            if (Handle<Node> node = value.Lock())
            {
                text->SetText(GetNodeName(node.Get()));
            }
            else
            {
                text->SetText(invalidNodeName);
            }
        }
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(WeakHandle<Node>, EditorWeakNodeFactory);

class EditorWeakSceneFactory : public UIElementFactory<WeakHandle<Scene>>
{
public:
    Handle<UIObject> Create(UIObject* parent, const WeakHandle<Scene>& value) const
    {
        String sceneName = "Invalid";
        UUID sceneUuid = UUID::Invalid();

        if (Handle<Scene> scene = value.Lock())
        {
            sceneName = scene->GetName().LookupString();
            sceneUuid = scene->GetUUID();
        }
        else
        {
            sceneUuid = UUID();
        }

        Handle<UIText> text = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
        text->SetText(sceneName);

        return text;
    }

    void Update(UIObject* uiObject, const WeakHandle<Scene>& value) const
    {
        static const String invalidSceneName = "<Invalid>";

        if (UIText* text = dynamic_cast<UIText*>(uiObject))
        {
            if (Handle<Scene> scene = value.Lock())
            {
                text->SetText(scene->GetName().LookupString());
            }
            else
            {
                text->SetText(invalidSceneName);
            }
        }
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(WeakHandle<Scene>, EditorWeakSceneFactory);

#if 0 // TODO revisit
class EntityUIElementFactory : public UIElementFactory<Handle<Entity>>
{
public:
    Handle<UIObject> Create(UIObject* parent, const Handle<Entity>& entity) const
    {
        const EditorNodePropertyRef* context = GetContext<EditorNodePropertyRef>();
        Assert(context != nullptr);

        if (!entity.IsValid())
        {
            Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            Handle<UIGridRow> row = grid->AddRow();
            Handle<UIGridColumn> column = row->AddColumn();

            Handle<UIButton> addEntityButton = parent->CreateUIObject<UIButton>(NAME("Add_Entity_Button"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));
            addEntityButton->SetText("Add Entity");

            addEntityButton->OnClick
                .Bind([world = parent->GetWorld()->HandleFromThis(), nodeWeak = context->node](...) -> UIEventHandlerResult
                    {
                        HYP_LOG(Editor, Debug, "Add Entity clicked");

                        if (Handle<Node> node = nodeWeak.Lock())
                        {
                            world->GetSubsystem<EditorSubsystem>()->GetCurrentProject()->GetActionStack()->Push(CreateObject<FunctionalEditorAction>(
                                NAME("AddEntity"),
                                [node, entity = Handle<Entity>::empty]() mutable -> EditorActionFunctions
                                {
                                    return {
                                        [&](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                                        {
                                            Scene* scene = node->GetScene();

                                            if (!scene)
                                            {
                                                HYP_LOG(Editor, Error, "GetScene() returned null for Node with name \"{}\", cannot add Entity", node->GetName());

                                                return;
                                            }

                                            if (!entity.IsValid())
                                            {
                                                entity = scene->GetEntityManager()->AddEntity();
                                            }

                                            node->SetEntity(entity);
                                        },
                                        [&](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                                        {
                                            node->SetEntity(Handle<Entity>::empty);
                                        }
                                    };
                                }));

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        HYP_LOG(Editor, Error, "Cannot add Entity to Node, Node reference could not be obtained");

                        return UIEventHandlerResult::ERR;
                    })
                .Detach();

            column->AddChildUIObject(addEntityButton);

            return grid;
        }

        EntityManager* entityManager = entity->GetEntityManager();

        if (!entityManager)
        {
            HYP_LOG(Editor, Error, "No EntityManager found for Entity {}", entity->Id());

            return nullptr;
        }

        auto createComponentsGrid = [&]() -> Handle<UIObject>
        {
            Optional<const TypeMap<ComponentId>&> allComponents = entityManager->GetAllComponents(entity);

            if (!allComponents.HasValue())
            {
                HYP_LOG(Editor, Error, "No component map found for Entity {}", entity->Id());

                return nullptr;
            }

            Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Name::Unique("EditorComponentsGrid"), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            for (const auto& it : *allComponents)
            {
                const TypeId componentTypeId = it.first;

                const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

                if (!componentInterface)
                {
                    HYP_LOG(Editor, Error, "No ComponentInterface registered for component with TypeId {}", componentTypeId.Value());

                    continue;
                }

                if (componentInterface->IsEntityTag())
                {
                    continue;
                }

                ComponentContainerBase* componentContainer = entityManager->TryGetContainer(componentTypeId);
                Assert(componentContainer != nullptr);

                HypData componentHypData;

                if (!componentContainer->TryGetComponent(it.second, componentHypData))
                {
                    HYP_LOG(Editor, Error, "Failed to get component of type \"{}\" with Id {} for Entity {}", componentInterface->GetTypeName(), it.second, entity->Id());

                    continue;
                }

                const HypClass* propertyPanelClass = nullptr;

                if (componentInterface->GetClass())
                {
                    if (!componentInterface->GetClass()->GetAttribute("editor", true))
                    {
                        // Skip components that are not meant to be edited in the editor
                        continue;
                    }

                    const HypClassAttributeValue& attr = componentInterface->GetClass()->GetAttribute("editorpropertypanelclass");

                    if (attr.IsValid())
                    {
                        propertyPanelClass = GetClass(CreateNameFromDynamicString(attr.GetString()));

                        if (!propertyPanelClass)
                        {
                            HYP_LOG(Editor, Error, "No HypClass registered for editor property panel class \"{}\"", attr.GetString());

                            continue;
                        }
                    }
                }

                Handle<UIObject> element;

                if (propertyPanelClass)
                {
                    if (!propertyPanelClass->IsDerivedFrom(EditorPropertyPanelBase::Class()))
                    {
                        HYP_LOG(Editor, Error, "Editor property panel class \"{}\" does not inherit from EditorPropertyPanelBase", propertyPanelClass->GetName());

                        continue;
                    }

                    // HypData propertyPanelInstance;
                    // propertyPanelClass->CreateInstance(propertyPanelInstance);

                    // if (!propertyPanelInstance.IsValid()) {
                    //     HYP_LOG(Editor, Error, "Failed to create instance of editor property panel class \"{}\"", propertyPanelClass->GetName());

                    //     continue;
                    // }

                    Handle<UIObject> propertyPanel = parent->CreateUIObject(propertyPanelClass, Name::Unique(propertyPanelClass->GetName().LookupString()), Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

                    if (!propertyPanel)
                    {
                        HYP_LOG(Editor, Error, "Failed to create editor property panel instance of class \"{}\"", propertyPanelClass->GetName());

                        continue;
                    }

                    Handle<EditorPropertyPanelBase> propertyPanelCasted = ObjCast<EditorPropertyPanelBase>(std::move(propertyPanel));

                    if (!propertyPanelCasted)
                    {
                        HYP_LOG(Editor, Error, "Failed to cast editor property panel instance to EditorPropertyPanelBase");

                        continue;
                    }

                    propertyPanelCasted->Build(componentHypData);

                    element = std::move(propertyPanelCasted);
                }
                else
                {
                    Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory(componentTypeId);

                    if (!factory)
                    {
                        HYP_LOG(Editor, Error, "No editor UI component factory registered for component of type \"{}\"", componentInterface->GetTypeName());

                        continue;
                    }

                    element = factory->CreateUIObject(parent, componentHypData, {});
                }

                if (!element)
                {
                    HYP_LOG(Editor, Error, "Failed to create UI object for component of type \"{}\"", componentInterface->GetTypeName());

                    continue;
                }

                Handle<UIGridRow> headerRow = grid->AddRow();
                Handle<UIGridColumn> headerColumn = headerRow->AddColumn();

                Handle<UIText> componentHeader = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

                Optional<String> componentHeaderTextOpt;
                Optional<String> componentDescriptionOpt;

                if (componentInterface->GetClass())
                {
                    if (const HypClassAttributeValue& attr = componentInterface->GetClass()->GetAttribute("label"))
                    {
                        componentHeaderTextOpt = attr.GetString();
                    }

                    if (const HypClassAttributeValue& attr = componentInterface->GetClass()->GetAttribute("description"))
                    {
                        componentDescriptionOpt = attr.GetString();
                    }
                }

                if (!componentHeaderTextOpt.HasValue())
                {
                    componentHeaderTextOpt = componentInterface->GetTypeName();
                }

                componentHeader->SetText(*componentHeaderTextOpt);
                componentHeader->SetTextSize(12);
                headerColumn->AddChildUIObject(componentHeader);

                if (componentDescriptionOpt.HasValue())
                {
                    Handle<UIGridRow> descriptionRow = grid->AddRow();
                    Handle<UIGridColumn> descriptionColumn = descriptionRow->AddColumn();

                    Handle<UIText> componentDescription = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                    componentDescription->SetTextSize(10);
                    componentDescription->SetText(*componentDescriptionOpt);

                    descriptionColumn->AddChildUIObject(componentDescription);
                }

                Handle<UIGridRow> contentRow = grid->AddRow();
                Handle<UIGridColumn> contentColumn = contentRow->AddColumn();

                Handle<UIPanel> componentContent = parent->CreateUIObject<UIPanel>(Vec2i { 0, 30 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

                componentContent->AddChildUIObject(element);

                contentColumn->AddChildUIObject(componentContent);
            }

            return grid;
        };

        Handle<UIGrid> componentsGridContainer = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        {
            Handle<UIGridRow> componentsGridContainerHeaderRow = componentsGridContainer->AddRow();

            {
                Handle<UIGridColumn> componentsGridContainerHeaderColumn = componentsGridContainerHeaderRow->AddColumn();
                componentsGridContainerHeaderColumn->SetColumnSize(6);

                Handle<UIText> componentsGridContainerHeaderText = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize({ 0, UIObjectSize::AUTO }, { 0, UIObjectSize::AUTO }));
                componentsGridContainerHeaderText->SetText("Components");
                componentsGridContainerHeaderColumn->AddChildUIObject(componentsGridContainerHeaderText);
            }

            {
                Handle<UIGridColumn> componentsGridContainerAddComponentButtonColumn = componentsGridContainerHeaderRow->AddColumn();
                componentsGridContainerAddComponentButtonColumn->SetColumnSize(6);

#if 0
                Handle<UIButton> addComponentButton = parent->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                addComponentButton->SetText("Add Component");
                addComponentButton->OnClick.Bind([stageWeak = parent->GetStage()->WeakHandleFromThis()](...)
                {
                    if (Handle<UIStage> stage = stageWeak.Lock())
                    {
                        auto loadedUiAsset = AssetManager::GetInstance()->Load<UIObject>("ui/dialog/Component.Add.ui.xml");
                        
                        if (loadedUiAsset.HasValue())
                        {
                            Handle<UIObject> loadedUi = loadedUiAsset->Result();

                            if (Handle<UIObject> addComponentWindow = loadedUi->FindChildUIObject("Add_Component_Window"))
                            {
                                stage->AddChildUIObject(addComponentWindow);
                        
                                return UIEventHandlerResult::STOP_BUBBLING;
                            }
                        }

                        HYP_LOG(Editor, Error, "Failed to load add component ui dialog! Error: {}", loadedUiAsset.GetError().GetMessage());

                        return UIEventHandlerResult::ERR;
                    }

                    return UIEventHandlerResult::ERR;
                }).Detach();

                componentsGridContainerAddComponentButtonColumn->AddChildUIObject(addComponentButton);
#endif
            }
        }

        {
            Handle<UIGridRow> componentsGridContainerScriptRow = componentsGridContainer->AddRow();
            Handle<UIGridColumn> componentsGridContainerScriptColumn = componentsGridContainerScriptRow->AddColumn();

#if 0
            // @TODO: Rewrite this once working
            if (entityManager->HasComponent<ScriptComponent>(entity)) {
                Handle<UIButton> editScriptButton = parent->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                editScriptButton->SetText("Edit Script");
                editScriptButton->OnClick.Bind([entity, entityManager](...)
                {
                    ScriptComponent *scriptComponent = entityManager->TryGetComponent<ScriptComponent>(entity);

                    if (!scriptComponent) {
                        HYP_LOG(Editor, Error, "Failed to get ScriptComponent for Entity {}", entity->Id());

                        return UIEventHandlerResult::ERR;
                    }

                    FilePath scriptFilepath(scriptComponent->script.path);

                    HYP_LOG(Editor, Debug, "Opening script file \"{}\" in editor", scriptFilepath);

                    if (!scriptFilepath.Exists()) {
                        HYP_LOG(Editor, Error, "Script file \"{}\" does not exist", scriptFilepath);

                        return UIEventHandlerResult::ERR;
                    }

                    return UIEventHandlerResult::STOP_BUBBLING;
                }).Detach();

                componentsGridContainerScriptColumn->AddChildUIObject(editScriptButton);
            } else {
                Handle<UIButton> attachScriptButton = parent->CreateUIObject<UIButton>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                attachScriptButton->SetText("Attach Script");
                attachScriptButton->OnClick.Bind([world = entityManager->GetWorld()->HandleFromThis(), entityManagerWeak = entityManager->ToWeak(), entity](...)
                {
                    Handle<EntityManager> entityManager = entityManagerWeak.Lock();
                    
                    if (!entityManager)
                    {
                        HYP_LOG(Editor, Error, "Failed to get EntityManager");

                        return UIEventHandlerResult::ERR;
                    }

                    const Handle<AssetRegistry> &assetRegistry = g_assetManager->GetAssetRegistry();

                    if (!assetRegistry)
                    {
                        HYP_LOG(Editor, Error, "Failed to get AssetRegistry");

                        return UIEventHandlerResult::ERR;
                    }

                    Handle<AssetPackage> scriptsPackage = assetRegistry->GetPackageFromPath("Scripts", true);
                    Assert(scriptsPackage.IsValid());

                    Handle<Script> script = CreateObject<Script>();

                    // @TODO: better name for script asset
                    TResult<Handle<AssetObject>> assetObjectResult = scriptsPackage->NewAssetObject(Name::Unique("TestScript"), script);

                    if (assetObjectResult.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to create new script asset: {}", assetObjectResult.GetError().GetMessage());

                        return UIEventHandlerResult::ERR;
                    }

                    Handle<AssetObject> assetObject = assetObjectResult.GetValue();
                    
                    if (Result saveResult = assetObject->Save(); saveResult.HasError()) {
                        HYP_LOG(Editor, Error, "Failed to save script asset: {}", saveResult.GetError().GetMessage());

                        return UIEventHandlerResult::ERR;
                    }
                    
                    if (entityManager->HasComponent<ScriptComponent>(entity)) {
                        entityManager->RemoveComponent<ScriptComponent>(entity);
                    }

                    ScriptComponent scriptComponent;
                    Memory::StrCpy(scriptComponent.script.path, assetObject->GetPath().Data(), sizeof(scriptComponent.script.path));

                    entityManager->AddComponent<ScriptComponent>(entity, scriptComponent);

                    return UIEventHandlerResult::STOP_BUBBLING;
                }).Detach();

                componentsGridContainerScriptColumn->AddChildUIObject(attachScriptButton);
            }
#endif
        }

        Handle<UIGridRow> componentsGridContainerContentRow = componentsGridContainer->AddRow();
        Handle<UIGridColumn> componentsGridContainerContentColumn = componentsGridContainerContentRow->AddColumn();

        if (Threads::IsOnThread(entityManager->GetOwnerThreadId()))
        {
            componentsGridContainerContentColumn->AddChildUIObject(createComponentsGrid());
        }
        else
        {
            HYP_NAMED_SCOPE("Awaiting async component UI element creation");

            Task<Handle<UIObject>> task = Threads::GetThread(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue(createComponentsGrid);

            componentsGridContainerContentColumn->AddChildUIObject(task.Await());
        }

        return componentsGridContainer;
    }

    void Update(UIObject* uiObject, const Handle<Entity>& entity) const
    {
        // @TODO

        HYP_NOT_IMPLEMENTED_VOID();
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(Handle<Entity>, EntityUIElementFactory);
#endif

class EditorNodePropertyFactory : public UIElementFactory<EditorNodePropertyRef>
{
public:
    Handle<UIObject> Create(UIObject* parent, const EditorNodePropertyRef& value) const
    {
        Handle<Node> node = value.node.Lock();

        if (!node)
        {
            HYP_LOG(Editor, Error, "Node reference is invalid, cannot create UI element for property \"{}\"", value.title);

            return nullptr;
        }

        Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory(value.property->GetTypeId());

        if (!factory)
        {
            HYP_LOG(Editor, Error, "No factory registered for TypeId {} when creating UI element for property \"{}\"", value.property->GetTypeId().Value(), value.title);

            return nullptr;
        }

        // Create panel
        Handle<UIPanel> panel = parent->CreateUIObject<UIPanel>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

        {
            Handle<UIGrid> grid = parent->CreateUIObject<UIGrid>(Vec2i { 0, 0 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            Handle<UIGridRow> headerRow = grid->AddRow();
            Handle<UIGridColumn> headerColumn = headerRow->AddColumn();

            Handle<UIText> componentHeader = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));

            componentHeader->SetText(value.title);
            componentHeader->SetTextSize(12);
            headerColumn->AddChildUIObject(componentHeader);

            if (value.description.HasValue())
            {
                Handle<UIGridRow> descriptionRow = grid->AddRow();
                Handle<UIGridColumn> descriptionColumn = descriptionRow->AddColumn();

                Handle<UIText> componentDescription = parent->CreateUIObject<UIText>(Vec2i { 0, 0 }, UIObjectSize(UIObjectSize::AUTO));
                componentDescription->SetTextSize(10);
                componentDescription->SetText(*value.description);

                descriptionColumn->AddChildUIObject(componentDescription);
            }

            Handle<UIGridRow> contentRow = grid->AddRow();
            Handle<UIGridColumn> contentColumn = contentRow->AddColumn();

            panel->AddChildUIObject(grid);
        }

        {
            Handle<UIPanel> content = parent->CreateUIObject<UIPanel>(NAME("PropertyPanel_Content"), Vec2i { 0, 25 }, UIObjectSize({ 100, UIObjectSize::PERCENT }, { 0, UIObjectSize::AUTO }));

            if (Handle<UIObject> element = factory->CreateUIObject(parent, value.property->Get(HypData(node)), HypData(AnyRef(const_cast<EditorNodePropertyRef&>(value)))))
            {
                content->AddChildUIObject(element);
            }

            panel->AddChildUIObject(content);
        }

        return panel;
    }

    void Update(UIObject* uiObject, const EditorNodePropertyRef& value) const
    {
        // // @TODO Implement without recreating the UI element

        // Handle<Node> nodeRc = value.node.Lock();
        // Assert(nodeRc != nullptr);

        // Handle<UIElementFactoryBase> factory = GetEditorUIElementFactory(value.property->GetTypeId());
        // Assert(factory != nullptr);

        // Handle<UIPanel> content = uiObject->FindChildUIObject(WeakName("PropertyPanel_Content")).Cast<UIPanel>();
        // Assert(content != nullptr);

        // content->RemoveAllChildUIObjects();

        // Handle<UIObject> element = factory->CreateUIObject(uiObject, value.property->Get(HypData(nodeRc)), AnyRef(const_cast<EditorNodePropertyRef&>(value)));
        // Assert(element != nullptr);

        // content->AddChildUIObject(element);
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(EditorNodePropertyRef, EditorNodePropertyFactory);

class AssetPackageUIElementFactory : public UIElementFactory<AssetPackage>
{
public:
    Handle<UIObject> Create(UIObject* parent, const AssetPackage& value) const
    {
        Handle<UIText> text = parent->CreateUIObject<UIText>();
        text->SetText(value.GetName().LookupString());

        parent->AddTag(NodeTag(NAME("AssetPackage"), value.BuildPackagePath()));

        return text;
    }

    void Update(UIObject* uiObject, const AssetPackage& value) const
    {
        uiObject->SetText(value.GetName().LookupString());
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(AssetPackage, AssetPackageUIElementFactory);

class AssetObjectUIElementFactory : public UIElementFactory<AssetObject>
{
public:
    Handle<UIObject> Create(UIObject* parent, const AssetObject& value) const
    {
        Handle<UIText> text = parent->CreateUIObject<UIText>();
        text->SetText(value.GetName().LookupString());

        parent->AddTag(NodeTag(NAME("AssetObject"), value.GetUUID()));

        return text;
    }

    void Update(UIObject* uiObject, const AssetObject& value) const
    {
        uiObject->SetText(value.GetName().LookupString());
    }
};

HYP_DEFINE_UI_ELEMENT_FACTORY(AssetObject, AssetObjectUIElementFactory);

} // namespace hyperion
