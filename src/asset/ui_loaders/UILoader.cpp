/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/ui_loaders/UILoader.hpp>

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

#include <util/json/JSON.hpp>

#include <core/containers/Stack.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/String.hpp>
#include <core/functional/Delegate.hpp>
#include <core/logging/Logger.hpp>
#include <core/Name.hpp>

#include <math/Vector2.hpp>

#include <algorithm>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

#define UI_OBJECT_CREATE_FUNCTION(name) \
    { \
        String(HYP_STR(name)).ToUpper(), \
        [](UIStage *stage, Name name, Vec2i position, UIObjectSize size) -> RC<UIObject> \
            { return stage->CreateUIObject<UI##name>(name, position, size, false); } \
    }

static const FlatMap<String, std::add_pointer_t<RC<UIObject>(UIStage *, Name, Vec2i, UIObjectSize)>> g_node_create_functions {
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

    for (uint i = 0; i < split.Size() && i < result.size; i++) {
        split[i] = split[i].Trimmed();

        result[i] = StringUtil::Parse<int32>(split[i]);
    }

    return result;
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

static Optional<Pair<int32, UIObjectSize::Flags>> ParseUIObjectSizeElement(String str)
{
    str = str.Trimmed();
    str = str.ToUpper();

    if (str == "AUTO") {
        return Pair<int32, UIObjectSize::Flags> { 0, UIObjectSize::AUTO };
    }

    if (str == "FILL") {
        return Pair<int32, UIObjectSize::Flags> { 100, UIObjectSize::FILL };
    }

    const SizeType percent_index = str.FindIndex("%");

    int32 parsed_int;

    if (percent_index != String::not_found) {
        String sub = str.Substr(0, percent_index);

        if (!StringUtil::Parse<int32>(sub, &parsed_int)) {
            return { };
        }

        return Pair<int32, UIObjectSize::Flags> { parsed_int, UIObjectSize::PERCENT };
    }

    if (!StringUtil::Parse<int32>(str, &parsed_int)) {
        return { };
    }

    return Pair<int32, UIObjectSize::Flags> { parsed_int, UIObjectSize::PIXEL };
}

static Optional<UIObjectSize> ParseUIObjectSize(const String &str)
{
    Array<String> split = str.Split(' ');

    if (split.Empty()) {
        return { };
    }

    if (split.Size() == 1) {
        Optional<Pair<int32, UIObjectSize::Flags>> parse_result = ParseUIObjectSizeElement(split[0]);

        if (!parse_result.HasValue()) {
            return { };
        }
        
        return UIObjectSize(*parse_result, *parse_result);
    }

    if (split.Size() == 2) {
        Optional<Pair<int32, UIObjectSize::Flags>> width_parse_result = ParseUIObjectSizeElement(split[0]);
        Optional<Pair<int32, UIObjectSize::Flags>> height_parse_result = ParseUIObjectSizeElement(split[1]);

        if (!width_parse_result.HasValue() || !height_parse_result.HasValue()) {
            return { };
        }
        
        return UIObjectSize(*width_parse_result, *height_parse_result);
    }

    return { };
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

            UIObjectSize size;

            if (const Pair<String, String> *it = attributes.TryGet("size")) {
                if (Optional<UIObjectSize> parsed_size = ParseUIObjectSize(it->second); parsed_size.HasValue()) {
                    size = *parsed_size;
                } else {
                    HYP_LOG(Assets, LogLevel::WARNING, "UI object has invalid size property: {}", it->second);
                }
            }

            RC<UIObject> ui_object = node_create_functions_it->second(m_ui_stage, ui_object_name, position, size);

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
                    HYP_LOG(Assets, LogLevel::WARNING, "UI object has invalid inner size property: {}", it->second);
                }
            }

            if (const Pair<String, String> *it = attributes.TryGet("maxsize")) {
                if (Optional<UIObjectSize> parsed_size = ParseUIObjectSize(it->second); parsed_size.HasValue()) {
                    ui_object->SetMaxSize(*parsed_size);
                } else {
                    HYP_LOG(Assets, LogLevel::WARNING, "UI object has invalid max size property: {}", it->second);
                }
            }

            for (const Pair<String, String> &attribute : attributes) {
                const String attribute_name_upper = attribute.first.ToUpper();

                if (attribute_name_upper.StartsWith("ON")) {
                    bool found = false;

                    const auto get_delegate_functions_mouse_it = g_get_delegate_functions_mouse.Find(attribute_name_upper);

                    if (get_delegate_functions_mouse_it != g_get_delegate_functions_mouse.End()) {
                        Delegate<UIEventHandlerResult, const MouseEvent &> *delegate = get_delegate_functions_mouse_it->second(ui_object.Get());

                        delegate->Bind(UIScriptDelegate< const MouseEvent & > { ui_object.Get(), attribute.second }).Detach();

                        found = true;
                    }

                    if (found) {
                        continue;
                    }

                    const auto get_delegate_functions_keyboard_it = g_get_delegate_functions_keyboard.Find(attribute_name_upper);

                    if (get_delegate_functions_keyboard_it != g_get_delegate_functions_keyboard.End()) {
                        Delegate<UIEventHandlerResult, const KeyboardEvent &> *delegate = get_delegate_functions_keyboard_it->second(ui_object.Get());

                        delegate->Bind(UIScriptDelegate< const KeyboardEvent & > { ui_object.Get(), attribute.second }).Detach();
                    }

                    if (found) {
                        continue;
                    }

                    HYP_LOG(Assets, LogLevel::WARNING, "Unknown event attribute: {}", attribute.first);
                } else if (!g_standard_ui_object_attributes.Contains(attribute_name_upper)) {
                    ui_object->SetNodeTag(CreateNameFromDynamicString(attribute.first.ToLower()), NodeTag(attribute.second));
                }
            }

            LastObject()->AddChildUIObject(ui_object.Get());

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
                HYP_LOG(Assets, LogLevel::WARNING, "Script node missing assembly or class attribute");
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
                HYP_LOG(Assets, LogLevel::WARNING, "Invalid UI object structure");

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

LoadedAsset UILoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    RC<UIObject> ui_stage(new UIStage(ThreadID::Current()));

    // temp
    AssertThrow(ui_stage.Is<UIStage>());

    ui_stage->Init();

    UISAXHandler handler(&state, static_cast<UIStage *>(ui_stage.Get()));

    xml::SAXParser parser(&handler);
    auto sax_result = parser.Parse(&state.stream);

    if (!sax_result) {
        HYP_LOG(Assets, LogLevel::WARNING, "Failed to parse UI stage: {}", sax_result.message);

        return { { LoaderResult::Status::ERR, sax_result.message } };
    }

    return { { LoaderResult::Status::OK }, ui_stage };
}

} // namespace hyperion
