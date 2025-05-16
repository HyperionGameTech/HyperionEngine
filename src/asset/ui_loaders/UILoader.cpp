/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/ui_loaders/UILoader.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <dotnet/DotNetSystem.hpp>
#include <dotnet/Class.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIObject.hpp>
#include <ui/UIText.hpp>
#include <ui/UIButton.hpp>
#include <ui/UIPanel.hpp>
#include <ui/UITabView.hpp>
#include <ui/UIMenuBar.hpp>
#include <ui/UIGrid.hpp>
#include <ui/UIImage.hpp>
#include <ui/UIDockableContainer.hpp>
#include <ui/UIListView.hpp>
#include <ui/UITextbox.hpp>
#include <ui/UIWindow.hpp>
#include <ui/UIScriptDelegate.hpp>

#include <util/xml/SAXParser.hpp>

#include <core/json/JSON.hpp>

#include <core/Core.hpp>

#include <core/containers/Stack.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/String.hpp>

#include <core/algorithm/Map.hpp>
#include <core/algorithm/FindIf.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypDataJSONHelpers.hpp>

#include <core/functional/Delegate.hpp>

#include <core/logging/Logger.hpp>

#include <core/Name.hpp>

#include <core/math/Vector2.hpp>

#include <algorithm>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

#define UI_OBJECT_CREATE_FUNCTION(name) \
    { \
        String(HYP_STR(name)).ToUpper(), \
        [](UIObject *parent, Name name, Vec2i position, UIObjectSize size) -> Pair<RC<UIObject>, const HypClass *> \
            { return { parent->CreateUIObject<UI##name>(name, position, size), GetClass<UI##name>() }; } \
    }

static const FlatMap<String, std::add_pointer_t<Pair<RC<UIObject>, const HypClass *>(UIObject *, Name, Vec2i, UIObjectSize)>> g_node_create_functions {
    UI_OBJECT_CREATE_FUNCTION(Button),
    UI_OBJECT_CREATE_FUNCTION(Text),
    UI_OBJECT_CREATE_FUNCTION(Panel),
    UI_OBJECT_CREATE_FUNCTION(Image),
    UI_OBJECT_CREATE_FUNCTION(TabView),
    UI_OBJECT_CREATE_FUNCTION(Tab),
    UI_OBJECT_CREATE_FUNCTION(Grid),
    UI_OBJECT_CREATE_FUNCTION(GridRow),
    UI_OBJECT_CREATE_FUNCTION(GridColumn),
    UI_OBJECT_CREATE_FUNCTION(MenuBar),
    UI_OBJECT_CREATE_FUNCTION(MenuItem),
    UI_OBJECT_CREATE_FUNCTION(DockableContainer),
    UI_OBJECT_CREATE_FUNCTION(DockableItem),
    UI_OBJECT_CREATE_FUNCTION(ListView),
    UI_OBJECT_CREATE_FUNCTION(ListViewItem),
    UI_OBJECT_CREATE_FUNCTION(Textbox),
    UI_OBJECT_CREATE_FUNCTION(Window)
};

#undef UI_OBJECT_CREATE_FUNCTION

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name) \
    { \
        String(HYP_STR(name)).ToUpper(), \
        [](UIObject *ui_object) -> Delegate<UIEventHandlerResult> * \
            { return &ui_object->name; } \
    }

static const FlatMap<String, std::add_pointer_t< Delegate<UIEventHandlerResult> *(UIObject *)> > g_get_delegate_functions {
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnInit),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnAttached),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnRemoved)
};

#undef UI_OBJECT_GET_DELEGATE_FUNCTION

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name) \
    { \
        String(HYP_STR(name)).ToUpper(), \
        [](UIObject *ui_object) -> Delegate<UIEventHandlerResult, UIObject *> * \
            { return &ui_object->name; } \
    }

static const FlatMap<String, std::add_pointer_t< Delegate<UIEventHandlerResult, UIObject *> *(UIObject *)> > g_get_delegate_functions_children {
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnChildAttached),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnChildRemoved)
};

#undef UI_OBJECT_GET_DELEGATE_FUNCTION

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name) \
    { \
        String(HYP_STR(name)).ToUpper(), \
        [](UIObject *ui_object) -> Delegate<UIEventHandlerResult, const MouseEvent &> * \
            { return &ui_object->name; } \
    }

static const FlatMap<String, std::add_pointer_t< Delegate<UIEventHandlerResult, const MouseEvent &> *(UIObject *)> > g_get_delegate_functions_mouse {
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnMouseDown),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnMouseUp),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnMouseDrag),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnMouseHover),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnMouseLeave),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnMouseMove),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnGainFocus),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnLoseFocus),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnScroll),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnClick)
};

#undef UI_OBJECT_GET_DELEGATE_FUNCTION

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name) \
    { \
        String(HYP_STR(name)).ToUpper(), \
        [](UIObject *ui_object) -> Delegate<UIEventHandlerResult, const KeyboardEvent &> * \
            { return &ui_object->name; } \
    }

static const FlatMap<String, std::add_pointer_t< Delegate<UIEventHandlerResult, const KeyboardEvent &> *(UIObject *)> > g_get_delegate_functions_keyboard {
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnKeyDown),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnKeyUp)
};

#undef UI_OBJECT_GET_DELEGATE_FUNCTION

static const HashMap<String, UIObjectAlignment> g_ui_alignment_strings {
    { "TOPLEFT", UIObjectAlignment::TOP_LEFT },
    { "TOPRIGHT", UIObjectAlignment::TOP_RIGHT },
    { "CENTER", UIObjectAlignment::CENTER },
    { "BOTTOMLEFT", UIObjectAlignment::BOTTOM_LEFT },
    { "BOTTOMRIGHT", UIObjectAlignment::BOTTOM_RIGHT }
};

static const Array<String> g_standard_ui_object_attributes {
    "NAME",
    "POSITION",
    "SIZE",
    "INNERSIZE",
    "MAXSIZE",
    "PARENTALIGNMENT",
    "ORIGINALIGNMENT",
    "VISIBLE",
    "PADDING",
    "TEXT",
    "TEXTSIZE",
    "TEXTCOLOR",
    "BACKGROUNDCOLOR",
    "DEPTH"
};

static UIObjectAlignment ParseUIObjectAlignment(const String &str)
{
    const String str_upper = str.ToUpper();

    const auto alignment_it = g_ui_alignment_strings.Find(str_upper);

    if (alignment_it != g_ui_alignment_strings.End()) {
        return alignment_it->second;
    }

    return UIObjectAlignment::TOP_LEFT;
}

static Vec2i ParseVec2i(const String &str)
{
    Vec2i result = Vec2i::Zero();

    Array<String> split = str.Split(' ');

    for (uint32 i = 0; i < split.Size() && i < result.size; i++) {
        split[i] = split[i].Trimmed();

        result[i] = StringUtil::Parse<int32>(split[i]);
    }

    return result;
}

static Optional<float> ParseFloat(const String &str)
{
    float value = 0.0f;

    if (!StringUtil::Parse(str, &value)) {
        return { };
    }

    return value;
}

static Optional<Color> ParseColor(const String &str)
{
    uint8 values[4] = { 0, 0, 0, 255 };

    // Parse hex if it starts with #
    if (str.StartsWith("#")) {
        int value_index = 0;

        for (int i = 1; i < str.Length() && value_index < 4; i += 2, value_index++) {
            const String substr = str.Substr(i, i + 2);
            const long value = std::strtol(substr.Data(), nullptr, 16);

            if (uint32(value) >= 256) {
                return { };
            }
            
            values[value_index] = uint8(value);
        }
    } else {
        auto fn = FunctionWrapper<String(String::*)() const>(&String::Trimmed);

        // Parse rgba if csv
        Array<String> split = Map(str.Split(','), &String::Trimmed);

        if (split.Size() == 4) {
        } else if (split.Size() == 3) {
            split.PushBack("255");
        } else {
            return { };
        }

        if (!StringUtil::Parse<uint8>(split[0], &values[0]) ||
            !StringUtil::Parse<uint8>(split[1], &values[1]) ||
            !StringUtil::Parse<uint8>(split[2], &values[2]) ||
            !StringUtil::Parse<uint8>(split[3], &values[3])) {
            return { };
        }
    }

    return Color(Vec4f { float(values[0]) / 255.0f, float(values[1]) / 255.0f, float(values[2]) / 255.0f, float(values[3]) / 255.0f });
}

static Optional<bool> ParseBool(const String &str)
{
    const String str_upper = str.ToUpper();

    if (str_upper == "TRUE") {
        return true;
    }

    if (str_upper == "FALSE") {
        return false;
    }

    return { };
}

static Optional<Pair<int32, uint32>> ParseUIObjectSizeElement(String str)
{
    str = str.Trimmed();
    str = str.ToUpper();

    if (str == "AUTO") {
        return Pair<int32, uint32> { 0, UIObjectSize::AUTO };
    }

    if (str == "FILL") {
        return Pair<int32, uint32> { 100, UIObjectSize::FILL };
    }

    const SizeType percent_index = str.FindFirstIndex("%");

    int32 parsed_int;

    if (percent_index != String::not_found) {
        String sub = str.Substr(0, percent_index);

        if (!StringUtil::Parse<int32>(sub, &parsed_int)) {
            return { };
        }

        return Pair<int32, uint32> { parsed_int, UIObjectSize::PERCENT };
    }

    if (!StringUtil::Parse<int32>(str, &parsed_int)) {
        return { };
    }

    return Pair<int32, uint32> { parsed_int, UIObjectSize::PIXEL };
}

static Optional<UIObjectSize> ParseUIObjectSize(const String &str)
{
    Array<String> split = str.Split(' ');

    if (split.Empty()) {
        return { };
    }

    if (split.Size() == 1) {
        Optional<Pair<int32, uint32>> parse_result = ParseUIObjectSizeElement(split[0]);

        if (!parse_result.HasValue()) {
            return { };
        }
        
        return UIObjectSize(*parse_result, *parse_result);
    }

    if (split.Size() == 2) {
        Optional<Pair<int32, uint32>> width_parse_result = ParseUIObjectSizeElement(split[0]);
        Optional<Pair<int32, uint32>> height_parse_result = ParseUIObjectSizeElement(split[1]);

        if (!width_parse_result.HasValue() || !height_parse_result.HasValue()) {
            return { };
        }
        
        return UIObjectSize(*width_parse_result, *height_parse_result);
    }

    return { };
}

static json::ParseResult ParseJSON(fbom::FBOMLoadContext &context, const String &str, fbom::FBOMData &out_data)
{
    // Read string as JSON
    json::ParseResult parse_result = json::JSON::Parse(str);

    if (!parse_result.ok) {
        return parse_result;
    }

    out_data = fbom::FBOMData::FromJSON(parse_result.value);

    return parse_result;
}

class UISAXHandler : public xml::SAXHandler
{
public:
    UISAXHandler(LoaderState *state, UIStage *ui_stage)
        : m_ui_stage(ui_stage)
    {
        AssertThrow(ui_stage != nullptr);

        m_ui_object_stack.Push(ui_stage);
    }

    UIObject *LastObject()
    {
        AssertThrow(m_ui_object_stack.Any());

        return m_ui_object_stack.Top();
    }

    virtual void Begin(const String &name, const xml::AttributeMap &attributes) override
    {
        UIObject *parent = LastObject();

        if (parent == nullptr) {
            parent = m_ui_stage;
        }

        AssertThrow(parent != nullptr);

        const String node_name_upper = name.ToUpper();

        const auto node_create_functions_it = g_node_create_functions.Find(node_name_upper);

        if (node_create_functions_it != g_node_create_functions.End()) {
            Name ui_object_name = Name::Invalid();

            if (const Pair<String, String> *it = attributes.TryGet("name")) {
                ui_object_name = CreateNameFromDynamicString(ANSIString(it->second));
            }

            Vec2i position = Vec2i::Zero();

            if (const Pair<String, String> *it = attributes.TryGet("position")) {
                position = ParseVec2i(it->second);
            }

            UIObjectSize size = UIObjectSize(UIObjectSize::AUTO);

            if (const Pair<String, String> *it = attributes.TryGet("size")) {
                if (Optional<UIObjectSize> parsed_size = ParseUIObjectSize(it->second); parsed_size.HasValue()) {
                    size = *parsed_size;
                } else {
                    HYP_LOG(Assets, Warning, "UI object has invalid size property: {}", it->second);
                }
            }

            Pair<RC<UIObject>, const HypClass *> create_result = node_create_functions_it->second(parent, ui_object_name, position, size);

            const RC<UIObject> &ui_object = create_result.first;
            const HypClass *hyp_class = ui_object->InstanceClass();

            // Set properties based on attributes
            if (const Pair<String, String> *it = attributes.TryGet("parentalignment")) {
                UIObjectAlignment alignment = ParseUIObjectAlignment(it->second);

                ui_object->SetParentAlignment(alignment);
            }

            if (const Pair<String, String> *it = attributes.TryGet("originalignment")) {
                UIObjectAlignment alignment = ParseUIObjectAlignment(it->second);

                ui_object->SetOriginAlignment(alignment);
            }

            if (const Pair<String, String> *it = attributes.TryGet("visible")) {
                if (Optional<bool> parsed_bool = ParseBool(it->second); parsed_bool.HasValue()) {
                    ui_object->SetIsVisible(*parsed_bool);
                }
            }

            if (const Pair<String, String> *it = attributes.TryGet("padding")) {
                ui_object->SetPadding(ParseVec2i(it->second));
            }

            if (const Pair<String, String> *it = attributes.TryGet("text")) {
                ui_object->SetText(it->second);
            }

            if (const Pair<String, String> *it = attributes.TryGet("depth")) {
                ui_object->SetDepth(StringUtil::Parse<int32>(it->second));
            }

            if (const Pair<String, String> *it = attributes.TryGet("innersize")) {
                if (Optional<UIObjectSize> parsed_size = ParseUIObjectSize(it->second); parsed_size.HasValue()) {
                    ui_object->SetInnerSize(*parsed_size);
                } else {
                    HYP_LOG(Assets, Warning, "UI object has invalid inner size property: {}", it->second);
                }
            }

            if (const Pair<String, String> *it = attributes.TryGet("maxsize")) {
                if (Optional<UIObjectSize> parsed_size = ParseUIObjectSize(it->second); parsed_size.HasValue()) {
                    ui_object->SetMaxSize(*parsed_size);
                } else {
                    HYP_LOG(Assets, Warning, "UI object has invalid max size property: {}", it->second);
                }
            }

            if (const Pair<String, String> *it = attributes.TryGet("backgroundcolor")) {
                if (Optional<Color> parsed_color = ParseColor(it->second); parsed_color.HasValue()) {
                    ui_object->SetBackgroundColor(*parsed_color);
                } else {
                    HYP_LOG(Assets, Warning, "UI object has invalid background color property: {}", it->second);
                }
            }

            if (const Pair<String, String> *it = attributes.TryGet("textcolor")) {
                if (Optional<Color> parsed_color = ParseColor(it->second); parsed_color.HasValue()) {
                    ui_object->SetTextColor(*parsed_color);
                } else {
                    HYP_LOG(Assets, Warning, "UI object has invalid text color property: {}", it->second);
                }
            }

            if (const Pair<String, String> *it = attributes.TryGet("textsize")) {
                if (Optional<float> parsed_float = ParseFloat(it->second); parsed_float.HasValue()) {
                    ui_object->SetTextSize(*parsed_float);
                } else {
                    HYP_LOG(Assets, Warning, "UI object has invalid text size property: {}", it->second);
                }
            }

            for (const Pair<String, String> &attribute : attributes) {
                const String attribute_name_upper = attribute.first.ToUpper();

                if (g_standard_ui_object_attributes.Contains(attribute_name_upper)) {
                    continue;
                }

                if (attribute_name_upper.StartsWith("TAG:")) {
                    // Strip the tag prefix
                    String attribute_name_lower = String(attribute_name_upper.Substr(4)).ToLower();

                    ui_object->SetNodeTag(NodeTag(CreateNameFromDynamicString(attribute_name_lower), attribute.second));

                    continue;
                }

                if (attribute_name_upper.StartsWith("ON")) {
                    bool found = false;

                    const auto get_delegate_functions_it = g_get_delegate_functions.Find(attribute_name_upper);

                    if (get_delegate_functions_it != g_get_delegate_functions.End()) {
                        Delegate<UIEventHandlerResult> *delegate = get_delegate_functions_it->second(ui_object.Get());

                        delegate->Bind(UIScriptDelegate< > { ui_object.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found) {
                        continue;
                    }

                    const auto get_delegate_functions_children_it = g_get_delegate_functions_children.Find(attribute_name_upper);

                    if (get_delegate_functions_children_it != g_get_delegate_functions_children.End()) {
                        Delegate<UIEventHandlerResult, UIObject *> *delegate = get_delegate_functions_children_it->second(ui_object.Get());

                        delegate->Bind(UIScriptDelegate<UIObject *> { ui_object.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found) {
                        continue;
                    }

                    const auto get_delegate_functions_mouse_it = g_get_delegate_functions_mouse.Find(attribute_name_upper);

                    if (get_delegate_functions_mouse_it != g_get_delegate_functions_mouse.End()) {
                        Delegate<UIEventHandlerResult, const MouseEvent &> *delegate = get_delegate_functions_mouse_it->second(ui_object.Get());

                        delegate->Bind(UIScriptDelegate<MouseEvent> { ui_object.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found) {
                        continue;
                    }

                    const auto get_delegate_functions_keyboard_it = g_get_delegate_functions_keyboard.Find(attribute_name_upper);

                    if (get_delegate_functions_keyboard_it != g_get_delegate_functions_keyboard.End()) {
                        Delegate<UIEventHandlerResult, const KeyboardEvent &> *delegate = get_delegate_functions_keyboard_it->second(ui_object.Get());

                        delegate->Bind(UIScriptDelegate<KeyboardEvent> { ui_object.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found) {
                        continue;
                    }

                    HYP_LOG(Assets, Warning, "Unknown event attribute: {}", attribute.first);
                }
                
                const String attribute_name_lower = attribute.first.ToLower();

                // Check HypClass attributes
                auto HandleFoundMember = [ui_object](const IHypMember &member, const String &str) -> bool
                {
                    HypData data;
                    json::ParseResult json_parse_result = json::JSON::Parse(str);

                    if (json_parse_result.ok) {
                        if (!JSONToHypData(json_parse_result.value, member.GetTypeID(), data)) {
                            HYP_LOG(Assets, Error, "Failed to deserialize field \"{}\" of HypClass \"{}\" from JSON",
                                member.GetName(), ui_object->GetName());

                            return false;
                        }
                    } else {
                        HYP_LOG(Assets, Error, "Failed to parse JSON for field \"{}\" of HypClass \"{}\": {}",
                            member.GetName(), ui_object->GetName(), json_parse_result.message);

                        return false;
                    }

                    HypData target_value { ui_object };

                    switch (member.GetMemberType()) {
                    case HypMemberType::TYPE_FIELD:
                    {
                        const HypField *hyp_field = dynamic_cast<const HypField *>(&member);

                        if (!hyp_field) {
                            HYP_LOG(Assets, Error, "Cannot set HypClass field: {}", member.GetName());

                            return false;
                        }

                        hyp_field->Set(target_value, data);

                        return true;
                    }
                    case HypMemberType::TYPE_PROPERTY:
                    {
                        const HypProperty *hyp_property = dynamic_cast<const HypProperty *>(&member);

                        if (!hyp_property || !hyp_property->CanSet()) {
                            HYP_LOG(Assets, Error, "Cannot set HypClass property: {}", member.GetName());

                            return false;
                        }

                        hyp_property->Set(target_value, data);

                        return true;
                    }
                    default:
                        HYP_UNREACHABLE();
                    }

                    return false;
                };
                
                { // find XMLAttribute member
                    HypClassMemberList member_list = hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD);

                    auto member_it = FindIf(member_list.Begin(), member_list.End(), [&HandleFoundMember, &attribute_name_lower](const auto &it)
                    {
                        if (const HypClassAttributeValue &attr = it.GetAttribute("xmlattribute"); attr.IsValid()) {
                            return attr.GetString().ToLower() == attribute_name_lower;
                        }

                        return false;
                    });

                    if (member_it != member_list.End()) {
                        if (!HandleFoundMember(*member_it, attribute.second)) {
                            HYP_LOG(Assets, Error, "Failed to set attribute {} on UIObject {}", attribute_name_lower, ui_object->GetName());
                        }

                        continue;
                    }
                }

                // Find property with name matching, without xmlattribute attribute
                { // find XMLAttribute member
                    HypClassMemberList member_list = hyp_class->GetMembers(HypMemberType::TYPE_PROPERTY);

                    auto member_it = FindIf(member_list.Begin(), member_list.End(), [&HandleFoundMember, &attribute_name_lower](const auto &it)
                    {
                        if (it.GetAttribute("xmlattribute").IsValid()) {
                            return false;
                        }

                        if (String(it.GetName().LookupString()).ToLower() == attribute_name_lower) {
                            return true;
                        }

                        return false;
                    });

                    if (member_it != member_list.End()) {
                        if (!HandleFoundMember(*member_it, attribute.second)) {
                            HYP_LOG(Assets, Error, "Failed to set attribute {} on UIObject {}", attribute_name_lower, ui_object->GetName());
                        }

                        continue;
                    }
                }

                HYP_LOG(Assets, Warning, "Unknown attribute: {}", attribute.first);
            }

            LastObject()->AddChildUIObject(ui_object);

            m_ui_object_stack.Push(ui_object.Get());
        } else if (node_name_upper == "SCRIPT") {
            const Pair<String, String> *assembly_it = attributes.TryGet("assembly");
            const Pair<String, String> *class_it = attributes.TryGet("class");

            if (assembly_it && class_it) {
                ScriptComponent script_component { };
                Memory::StrCpy(script_component.script.assembly_path, assembly_it->second.Data(), ArraySize(script_component.script.assembly_path));
                Memory::StrCpy(script_component.script.class_name, class_it->second.Data(), ArraySize(script_component.script.class_name));

                if (m_ui_object_stack.Any()) {
                    LastObject()->SetScriptComponent(std::move(script_component));
                }
            } else {
                HYP_LOG(Assets, Warning, "Script node missing assembly or class attribute");
            }
        }
    }

    virtual void End(const String &name) override
    {
        const String node_name_upper = name.ToUpper();

        const auto node_create_functions_it = g_node_create_functions.Find(node_name_upper);

        if (node_create_functions_it != g_node_create_functions.End()) {
            UIObject *last_object = LastObject();

            // @TODO: Type check to ensure proper structure.

            // must always have one object in stack.
            if (m_ui_object_stack.Size() <= 1) {
                HYP_LOG(Assets, Warning, "Invalid UI object structure");

                return;
            }

            m_ui_object_stack.Pop();

            return;
        }
    }

    virtual void Characters(const String &value) override
    {
        UIObject *last_object = LastObject();

        last_object->SetText(value);
    }

    virtual void Comment(const String &comment) override {}

private:
    UIStage             *m_ui_stage;
    Stack<UIObject *>   m_ui_object_stack;
};

AssetLoadResult UILoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    RC<UIObject> ui_stage = MakeRefCountedPtr<UIStage>(ThreadID::Current());
    ui_stage->Init();

    UISAXHandler handler(&state, static_cast<UIStage *>(ui_stage.Get()));

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        HYP_LOG(Assets, Warning, "Failed to parse UI stage: {}", sax_result.message);

        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse XML: {}", sax_result.message);
    }

    return LoadedAsset { std::move(ui_stage) };
}

} // namespace hyperion
