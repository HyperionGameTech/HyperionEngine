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
#include <ui/UISpacer.hpp>
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

#include <core/containers/Stack.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/String.hpp>

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

#define UI_OBJECT_CREATE_FUNCTION(name)                                                                               \
    {                                                                                                                 \
        String(HYP_STR(name)).ToUpper(),                                                                              \
        [](UIObject* parent, Name name, Vec2i position, UIObjectSize size) -> Pair<Handle<UIObject>, const HypClass*> \
        {                                                                                                             \
            return { parent->CreateUIObject<UI##name>(name, position, size), GetClass<UI##name>() };                  \
        }                                                                                                             \
    }

static const FlatMap<String, std::add_pointer_t<Pair<Handle<UIObject>, const HypClass*>(UIObject*, Name, Vec2i, UIObjectSize)>> g_nodeCreateFunctions {
    UI_OBJECT_CREATE_FUNCTION(Stage),
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
    UI_OBJECT_CREATE_FUNCTION(Spacer),
    UI_OBJECT_CREATE_FUNCTION(DockableContainer),
    UI_OBJECT_CREATE_FUNCTION(DockableItem),
    UI_OBJECT_CREATE_FUNCTION(ListView),
    UI_OBJECT_CREATE_FUNCTION(ListViewItem),
    UI_OBJECT_CREATE_FUNCTION(Textbox),
    UI_OBJECT_CREATE_FUNCTION(Window)
};

#undef UI_OBJECT_CREATE_FUNCTION

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name)                               \
    {                                                                       \
        String(HYP_STR(name)).ToUpper(),                                    \
        [](UIObject* uiObject) -> ScriptableDelegate<UIEventHandlerResult>* \
        {                                                                   \
            return &uiObject->name;                                         \
        }                                                                   \
    }

static const FlatMap<String, std::add_pointer_t<ScriptableDelegate<UIEventHandlerResult>*(UIObject*)>> g_getDelegateFunctions {
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnInit),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnAttached),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnRemoved)
};

#undef UI_OBJECT_GET_DELEGATE_FUNCTION

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name)                                          \
    {                                                                                  \
        String(HYP_STR(name)).ToUpper(),                                               \
        [](UIObject* uiObject) -> ScriptableDelegate<UIEventHandlerResult, UIObject*>* \
        {                                                                              \
            return &uiObject->name;                                                    \
        }                                                                              \
    }

static const FlatMap<String, std::add_pointer_t<ScriptableDelegate<UIEventHandlerResult, UIObject*>*(UIObject*)>> g_getDelegateFunctionsChildren {
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnChildAttached),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnChildRemoved)
};

#undef UI_OBJECT_GET_DELEGATE_FUNCTION

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name)                                                  \
    {                                                                                          \
        String(HYP_STR(name)).ToUpper(),                                                       \
        [](UIObject* uiObject) -> ScriptableDelegate<UIEventHandlerResult, const MouseEvent&>* \
        {                                                                                      \
            return &uiObject->name;                                                            \
        }                                                                                      \
    }

static const FlatMap<String, std::add_pointer_t<ScriptableDelegate<UIEventHandlerResult, const MouseEvent&>*(UIObject*)>> g_getDelegateFunctionsMouse {
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

#define UI_OBJECT_GET_DELEGATE_FUNCTION(name)                                                     \
    {                                                                                             \
        String(HYP_STR(name)).ToUpper(),                                                          \
        [](UIObject* uiObject) -> ScriptableDelegate<UIEventHandlerResult, const KeyboardEvent&>* \
        {                                                                                         \
            return &uiObject->name;                                                               \
        }                                                                                         \
    }

static const FlatMap<String, std::add_pointer_t<ScriptableDelegate<UIEventHandlerResult, const KeyboardEvent&>*(UIObject*)>> g_getDelegateFunctionsKeyboard {
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnKeyDown),
    UI_OBJECT_GET_DELEGATE_FUNCTION(OnKeyUp)
};

#undef UI_OBJECT_GET_DELEGATE_FUNCTION

static const HashMap<String, UIObjectAlignment> g_uiAlignmentStrings {
    { "TOPLEFT", UIObjectAlignment::TOP_LEFT },
    { "TOPRIGHT", UIObjectAlignment::TOP_RIGHT },
    { "CENTER", UIObjectAlignment::CENTER },
    { "BOTTOMLEFT", UIObjectAlignment::BOTTOM_LEFT },
    { "BOTTOMRIGHT", UIObjectAlignment::BOTTOM_RIGHT }
};

static const Array<String> g_standardUiObjectAttributes {
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

static UIObjectAlignment ParseUIObjectAlignment(const String& str)
{
    const String strUpper = str.ToUpper();

    const auto alignmentIt = g_uiAlignmentStrings.Find(strUpper);

    if (alignmentIt != g_uiAlignmentStrings.End())
    {
        return alignmentIt->second;
    }

    return UIObjectAlignment::TOP_LEFT;
}

static Vec2i ParseVec2i(const String& str)
{
    Vec2i result = Vec2i::Zero();

    Array<String> split = str.Split(' ');

    for (uint32 i = 0; i < split.Size() && i < result.size; i++)
    {
        split[i] = split[i].Trimmed();

        result[i] = StringUtil::Parse<int32>(split[i]);
    }

    return result;
}

static Optional<float> ParseFloat(const String& str)
{
    float value = 0.0f;

    if (!StringUtil::Parse(str, &value))
    {
        return {};
    }

    return value;
}

static Optional<Color> ParseColor(const String& str)
{
    uint8 values[4] = { 0, 0, 0, 255 };

    // Parse hex if it starts with #
    if (str.StartsWith("#"))
    {
        int valueIndex = 0;

        for (int i = 1; i < str.Length() && valueIndex < 4; i += 2, valueIndex++)
        {
            const String substr = str.Substr(i, i + 2);
            const long value = std::strtol(substr.Data(), nullptr, 16);

            if (uint32(value) >= 256)
            {
                return {};
            }

            values[valueIndex] = uint8(value);
        }
    }
    else
    {
        auto fn = FunctionWrapper<String (String::*)() const>(&String::Trimmed);

        // Parse rgba if csv
        Array<String> split = Map(str.Split(','), &String::Trimmed);

        if (split.Size() == 4)
        {
        }
        else if (split.Size() == 3)
        {
            split.PushBack("255");
        }
        else
        {
            return {};
        }

        if (!StringUtil::Parse<uint8>(split[0], &values[0]) || !StringUtil::Parse<uint8>(split[1], &values[1]) || !StringUtil::Parse<uint8>(split[2], &values[2]) || !StringUtil::Parse<uint8>(split[3], &values[3]))
        {
            return {};
        }
    }

    return Color(Vec4f { float(values[0]) / 255.0f, float(values[1]) / 255.0f, float(values[2]) / 255.0f, float(values[3]) / 255.0f });
}

static Optional<bool> ParseBool(const String& str)
{
    const String strUpper = str.ToUpper();

    if (strUpper == "TRUE")
    {
        return true;
    }

    if (strUpper == "FALSE")
    {
        return false;
    }

    return {};
}

static Optional<Pair<int32, uint32>> ParseUIObjectSizeElement(String str)
{
    str = str.Trimmed();
    str = str.ToUpper();

    if (str == "AUTO")
    {
        return Pair<int32, uint32> { 0, UIObjectSize::AUTO };
    }

    if (str == "FILL")
    {
        return Pair<int32, uint32> { 100, UIObjectSize::FILL };
    }

    const SizeType percentIndex = str.FindFirstIndex("%");

    int32 parsedInt;

    if (percentIndex != String::notFound)
    {
        String sub = str.Substr(0, percentIndex);

        if (!StringUtil::Parse<int32>(sub, &parsedInt))
        {
            return {};
        }

        return Pair<int32, uint32> { parsedInt, UIObjectSize::PERCENT };
    }

    if (!StringUtil::Parse<int32>(str, &parsedInt))
    {
        return {};
    }

    return Pair<int32, uint32> { parsedInt, UIObjectSize::PIXEL };
}

static Optional<UIObjectSize> ParseUIObjectSize(const String& str)
{
    Array<String> split = str.Split(' ');

    if (split.Empty())
    {
        return {};
    }

    if (split.Size() == 1)
    {
        Optional<Pair<int32, uint32>> parseResult = ParseUIObjectSizeElement(split[0]);

        if (!parseResult.HasValue())
        {
            return {};
        }

        return UIObjectSize(*parseResult, *parseResult);
    }

    if (split.Size() == 2)
    {
        Optional<Pair<int32, uint32>> widthParseResult = ParseUIObjectSizeElement(split[0]);
        Optional<Pair<int32, uint32>> heightParseResult = ParseUIObjectSizeElement(split[1]);

        if (!widthParseResult.HasValue() || !heightParseResult.HasValue())
        {
            return {};
        }

        return UIObjectSize(*widthParseResult, *heightParseResult);
    }

    return {};
}

static json::ParseResult ParseJSON(FBOMLoadContext& context, const String& str, FBOMData& outData)
{
    // Read string as JSON
    json::ParseResult parseResult = json::JSON::Parse(str);

    if (!parseResult.ok)
    {
        return parseResult;
    }

    outData = FBOMData::FromJSON(parseResult.value);

    return parseResult;
}

class UISAXHandler : public xml::SAXHandler
{
public:
    UISAXHandler(LoaderState* state, UIStage* uiStage)
        : m_uiStage(uiStage)
    {
        Assert(uiStage != nullptr);

        m_uiObjectStack.Push(uiStage);
    }

    UIObject* LastObject()
    {
        Assert(m_uiObjectStack.Any());

        return m_uiObjectStack.Top();
    }

    virtual void Begin(const String& name, const xml::AttributeMap& attributes) override
    {
        UIObject* parent = LastObject();

        if (parent == nullptr)
        {
            parent = m_uiStage;
        }

        Assert(parent != nullptr);

        const String nodeNameUpper = name.ToUpper();

        const auto nodeCreateFunctionsIt = g_nodeCreateFunctions.Find(nodeNameUpper);

        if (nodeCreateFunctionsIt != g_nodeCreateFunctions.End())
        {
            Name uiObjectName = Name::Invalid();

            if (const Pair<String, String>* it = attributes.TryGet("name"))
            {
                uiObjectName = CreateNameFromDynamicString(ANSIString(it->second));
            }

            Vec2i position = Vec2i::Zero();

            if (const Pair<String, String>* it = attributes.TryGet("position"))
            {
                position = ParseVec2i(it->second);
            }

            UIObjectSize size = UIObjectSize(UIObjectSize::AUTO);

            if (const Pair<String, String>* it = attributes.TryGet("size"))
            {
                if (Optional<UIObjectSize> parsedSize = ParseUIObjectSize(it->second); parsedSize.HasValue())
                {
                    size = *parsedSize;
                }
                else
                {
                    HYP_LOG(Assets, Warning, "UI object has invalid size property: {}", it->second);
                }
            }

            Pair<Handle<UIObject>, const HypClass*> createResult = nodeCreateFunctionsIt->second(parent, uiObjectName, position, size);

            const Handle<UIObject>& uiObject = createResult.first;
            const HypClass* hypClass = uiObject->InstanceClass();

            // Set properties based on attributes
            if (const Pair<String, String>* it = attributes.TryGet("parentalignment"))
            {
                UIObjectAlignment alignment = ParseUIObjectAlignment(it->second);

                uiObject->SetParentAlignment(alignment);
            }

            if (const Pair<String, String>* it = attributes.TryGet("originalignment"))
            {
                UIObjectAlignment alignment = ParseUIObjectAlignment(it->second);

                uiObject->SetOriginAlignment(alignment);
            }

            if (const Pair<String, String>* it = attributes.TryGet("visible"))
            {
                if (Optional<bool> parsedBool = ParseBool(it->second); parsedBool.HasValue())
                {
                    uiObject->SetIsVisible(*parsedBool);
                }
            }

            if (const Pair<String, String>* it = attributes.TryGet("padding"))
            {
                uiObject->SetPadding(ParseVec2i(it->second));
            }

            if (const Pair<String, String>* it = attributes.TryGet("text"))
            {
                uiObject->SetText(it->second);
            }

            if (const Pair<String, String>* it = attributes.TryGet("depth"))
            {
                uiObject->SetDepth(StringUtil::Parse<int32>(it->second));
            }

            if (const Pair<String, String>* it = attributes.TryGet("innersize"))
            {
                if (Optional<UIObjectSize> parsedSize = ParseUIObjectSize(it->second); parsedSize.HasValue())
                {
                    uiObject->SetInnerSize(*parsedSize);
                }
                else
                {
                    HYP_LOG(Assets, Warning, "UI object has invalid inner size property: {}", it->second);
                }
            }

            if (const Pair<String, String>* it = attributes.TryGet("maxsize"))
            {
                if (Optional<UIObjectSize> parsedSize = ParseUIObjectSize(it->second); parsedSize.HasValue())
                {
                    uiObject->SetMaxSize(*parsedSize);
                }
                else
                {
                    HYP_LOG(Assets, Warning, "UI object has invalid max size property: {}", it->second);
                }
            }

            if (const Pair<String, String>* it = attributes.TryGet("backgroundcolor"))
            {
                if (Optional<Color> parsedColor = ParseColor(it->second); parsedColor.HasValue())
                {
                    uiObject->SetBackgroundColor(*parsedColor);
                }
                else
                {
                    HYP_LOG(Assets, Warning, "UI object has invalid background color property: {}", it->second);
                }
            }

            if (const Pair<String, String>* it = attributes.TryGet("textcolor"))
            {
                if (Optional<Color> parsedColor = ParseColor(it->second); parsedColor.HasValue())
                {
                    uiObject->SetTextColor(*parsedColor);
                }
                else
                {
                    HYP_LOG(Assets, Warning, "UI object has invalid text color property: {}", it->second);
                }
            }

            if (const Pair<String, String>* it = attributes.TryGet("textsize"))
            {
                if (Optional<float> parsedFloat = ParseFloat(it->second); parsedFloat.HasValue())
                {
                    uiObject->SetTextSize(*parsedFloat);
                }
                else
                {
                    HYP_LOG(Assets, Warning, "UI object has invalid text size property: {}", it->second);
                }
            }

            for (const Pair<String, String>& attribute : attributes)
            {
                const String& attributeName = attribute.first;
                const String& attributeValue = attribute.second;

                const String attributeNameUpper = attributeName.ToUpper();

                if (g_standardUiObjectAttributes.Contains(attributeNameUpper))
                {
                    continue;
                }

                if (attributeNameUpper.StartsWith("TAG:"))
                {
                    // Strip the tag prefix
                    String attributeNameLower = String(attributeNameUpper.Substr(4)).ToLower();

                    uiObject->SetNodeTag(NodeTag(CreateNameFromDynamicString(attributeNameLower), attribute.second));

                    continue;
                }

                if (attributeNameUpper.StartsWith("ON"))
                {
#if 1
                    // Find a ScriptableDelegate field with the name, bind C# function
                    HypClassMemberList memberList = hypClass->GetMembers(HypMemberType::TYPE_FIELD);

                    auto memberIt = FindIf(memberList.Begin(), memberList.End(), [&attributeNameUpper](const IHypMember& member)
                        {
                            const HypClassAttributeValue& attr = member.GetAttribute("scriptabledelegate");

                            if (!attr.GetBool())
                            {
                                return false;
                            }

                            if (String(member.GetName().LookupString()).ToUpper() == attributeNameUpper)
                            {
                                return true;
                            }

                            return false;
                        });

                    ScriptComponent* scriptComponent = uiObject->GetScriptComponent(true);

                    if (!scriptComponent)
                    {
                        HYP_LOG(Assets, Error, "Failed to bind \"{}\" event - No script component found on UI object \"{}\"",
                            attributeNameUpper, uiObject->GetName());

                        continue;
                    }

                    if (!scriptComponent->resource)
                    {
                        HYP_LOG(Assets, Error, "Failed to bind \"{}\" event - No ManagedObjectResource found on ScriptComponent for UIObject \"{}\"",
                            attributeNameUpper, uiObject->GetName());

                        continue;
                    }

                    // Bind a C# member function to the delegate
                    if (memberIt != memberList.End())
                    {
                        const HypField& field = static_cast<const HypField&>(*memberIt);

                        const uintptr_t fieldAddress = uintptr_t(uiObject.Get()) + uintptr_t(field.GetOffset());

                        IScriptableDelegate* scriptableDelegate = reinterpret_cast<IScriptableDelegate*>(fieldAddress);

                        scriptableDelegate
                            ->BindManaged(attributeValue,
                                [uiObjectWeak = uiObject->WeakHandleFromThis()]() -> ManagedObjectResource*
                                {
                                    Handle<UIObject> uiObject = uiObjectWeak.Lock();

                                    if (!uiObject)
                                    {
                                        return nullptr;
                                    }

                                    ScriptComponent* scriptComponent = uiObject->GetScriptComponent(true);

                                    if (!scriptComponent)
                                    {
                                        return nullptr;
                                    }

                                    return scriptComponent->resource;
                                })
                            .Detach();

                        continue;
                    }

#else
                    bool found = false;

                    const auto getDelegateFunctionsIt = g_getDelegateFunctions.Find(attributeNameUpper);

                    if (getDelegateFunctionsIt != g_getDelegateFunctions.End())
                    {
                        ScriptableDelegate<UIEventHandlerResult>* delegate = getDelegateFunctionsIt->second(uiObject.Get());

                        delegate->Bind(UIScriptDelegate<> { uiObject.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found)
                    {
                        continue;
                    }

                    const auto getDelegateFunctionsChildrenIt = g_getDelegateFunctionsChildren.Find(attributeNameUpper);

                    if (getDelegateFunctionsChildrenIt != g_getDelegateFunctionsChildren.End())
                    {
                        ScriptableDelegate<UIEventHandlerResult, UIObject*>* delegate = getDelegateFunctionsChildrenIt->second(uiObject.Get());

                        delegate->Bind(UIScriptDelegate<UIObject*> { uiObject.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found)
                    {
                        continue;
                    }

                    const auto getDelegateFunctionsMouseIt = g_getDelegateFunctionsMouse.Find(attributeNameUpper);

                    if (getDelegateFunctionsMouseIt != g_getDelegateFunctionsMouse.End())
                    {
                        ScriptableDelegate<UIEventHandlerResult, const MouseEvent&>* delegate = getDelegateFunctionsMouseIt->second(uiObject.Get());

                        delegate->Bind(UIScriptDelegate<MouseEvent> { uiObject.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found)
                    {
                        continue;
                    }

                    const auto getDelegateFunctionsKeyboardIt = g_getDelegateFunctionsKeyboard.Find(attributeNameUpper);

                    if (getDelegateFunctionsKeyboardIt != g_getDelegateFunctionsKeyboard.End())
                    {
                        ScriptableDelegate<UIEventHandlerResult, const KeyboardEvent&>* delegate = getDelegateFunctionsKeyboardIt->second(uiObject.Get());

                        delegate->Bind(UIScriptDelegate<KeyboardEvent> { uiObject.Get(), attribute.second, UIScriptDelegateFlags::ALLOW_NESTED }).Detach();

                        found = true;
                    }

                    if (found)
                    {
                        continue;
                    }
#endif

                    HYP_LOG(Assets, Warning, "Unknown event attribute: {}", attribute.first);
                }

                const String attributeNameLower = attribute.first.ToLower();

                // Check HypClass attributes
                auto HandleFoundMember = [uiObject](const IHypMember& member, const String& str) -> bool
                {
                    HypData data;
                    json::ParseResult jsonParseResult = json::JSON::Parse(str);

                    if (jsonParseResult.ok)
                    {
                        if (!JSONToHypData(jsonParseResult.value, member.GetTypeId(), data))
                        {
                            HYP_LOG(Assets, Error, "Failed to deserialize field \"{}\" of HypClass \"{}\" from JSON",
                                member.GetName(), uiObject->GetName());

                            return false;
                        }
                    }
                    else
                    {
                        HYP_LOG(Assets, Error, "Failed to parse JSON for field \"{}\" of HypClass \"{}\": {}",
                            member.GetName(), uiObject->GetName(), jsonParseResult.message);

                        return false;
                    }

                    HypData targetValue { uiObject };
                    Assert(targetValue.Is<UIObject*>());

                    switch (member.GetMemberType())
                    {
                    case HypMemberType::TYPE_FIELD:
                    {
                        const HypField* hypField = dynamic_cast<const HypField*>(&member);

                        if (!hypField)
                        {
                            HYP_LOG(Assets, Error, "Cannot set HypClass field: {}", member.GetName());

                            return false;
                        }

                        hypField->Set(targetValue, data);

                        return true;
                    }
                    case HypMemberType::TYPE_PROPERTY:
                    {
                        const HypProperty* hypProperty = dynamic_cast<const HypProperty*>(&member);

                        if (!hypProperty || !hypProperty->CanSet())
                        {
                            HYP_LOG(Assets, Error, "Cannot set HypClass property: {}", member.GetName());

                            return false;
                        }

                        hypProperty->Set(targetValue, data);

                        return true;
                    }
                    default:
                        HYP_UNREACHABLE();
                    }

                    return false;
                };

                { // find XMLAttribute member
                    HypClassMemberList memberList = hypClass->GetMembers(HypMemberType::TYPE_PROPERTY | HypMemberType::TYPE_FIELD);

                    auto memberIt = FindIf(memberList.Begin(), memberList.End(), [&HandleFoundMember, &attributeNameLower](const auto& it)
                        {
                            if (const HypClassAttributeValue& attr = it.GetAttribute("xmlattribute"); attr.IsValid())
                            {
                                return attr.GetString().ToLower() == attributeNameLower;
                            }

                            return false;
                        });

                    if (memberIt != memberList.End())
                    {
                        if (!HandleFoundMember(*memberIt, attribute.second))
                        {
                            HYP_LOG(Assets, Error, "Failed to set attribute {} on UIObject {}", attributeNameLower, uiObject->GetName());
                        }

                        continue;
                    }
                }

                // Find property with name matching, without xmlattribute attribute
                { // find XMLAttribute member
                    HypClassMemberList memberList = hypClass->GetMembers(HypMemberType::TYPE_PROPERTY);

                    auto memberIt = FindIf(memberList.Begin(), memberList.End(), [&HandleFoundMember, &attributeNameLower](const auto& it)
                        {
                            if (it.GetAttribute("xmlattribute").IsValid())
                            {
                                return false;
                            }

                            if (String(it.GetName().LookupString()).ToLower() == attributeNameLower)
                            {
                                return true;
                            }

                            return false;
                        });

                    if (memberIt != memberList.End())
                    {
                        if (!HandleFoundMember(*memberIt, attribute.second))
                        {
                            HYP_LOG(Assets, Error, "Failed to set attribute {} on UIObject {}", attributeNameLower, uiObject->GetName());
                        }

                        continue;
                    }
                }

                HYP_LOG(Assets, Warning, "Unknown attribute: {}", attribute.first);
            }

            LastObject()->AddChildUIObject(uiObject);

            m_uiObjectStack.Push(uiObject.Get());
        }
        else if (nodeNameUpper == "SCRIPT")
        {
            const Pair<String, String>* assemblyIt = attributes.TryGet("assembly");
            const Pair<String, String>* classIt = attributes.TryGet("class");

            if (assemblyIt && classIt)
            {
                ScriptComponent scriptComponent {};
                Memory::StrCpy(scriptComponent.script.assemblyPath, assemblyIt->second.Data(), ArraySize(scriptComponent.script.assemblyPath));
                Memory::StrCpy(scriptComponent.script.className, classIt->second.Data(), ArraySize(scriptComponent.script.className));

                if (m_uiObjectStack.Any())
                {
                    LastObject()->SetScriptComponent(std::move(scriptComponent));
                }
            }
            else
            {
                HYP_LOG(Assets, Warning, "Script node missing assembly or class attribute");
            }
        }
        else
        {
            HYP_LOG(Assets, Warning, "Unknown UI node: {}", name);
        }
    }

    virtual void End(const String& name) override
    {
        const String nodeNameUpper = name.ToUpper();

        const auto nodeCreateFunctionsIt = g_nodeCreateFunctions.Find(nodeNameUpper);

        if (nodeCreateFunctionsIt != g_nodeCreateFunctions.End())
        {
            UIObject* lastObject = LastObject();

            // @TODO: Type check to ensure proper structure.

            // must always have one object in stack.
            if (m_uiObjectStack.Size() <= 1)
            {
                HYP_LOG(Assets, Warning, "Invalid UI object structure");

                return;
            }

            m_uiObjectStack.Pop();

            return;
        }
    }

    virtual void Characters(const String& value) override
    {
        UIObject* lastObject = LastObject();

        lastObject->SetText(value);
    }

    virtual void Comment(const String& comment) override
    {
    }

private:
    UIStage* m_uiStage;
    Stack<UIObject*> m_uiObjectStack;
};

AssetLoadResult UILoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    Handle<UIObject> uiStage = CreateObject<UIStage>(ThreadId::Current());
    InitObject(uiStage);

    UISAXHandler handler(&state, static_cast<UIStage*>(uiStage.Get()));

    xml::SAXParser parser(&handler);
    auto saxResult = parser.Parse(&state.stream);

    if (!saxResult)
    {
        HYP_LOG(Assets, Warning, "Failed to parse UI stage: {}", saxResult.message);

        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse XML: {}", saxResult.message);
    }

    return LoadedAsset { std::move(uiStage) };
}

} // namespace hyperion
