/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Lexer.hpp>
#include <parser/Parser.hpp>

#include <Core/utilities/Format.hpp>
#include <Core/utilities/StringView.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/containers/HashSet.hpp>
#include <Core/containers/Forest.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/io/BufferedByteReader.hpp>

#include <Core/json/JSON.hpp>

#include <util/Util.hpp>

namespace Hyperion {
namespace CodeGen {

HYP_DECLARE_LOG_CHANNEL(Tool);

#define HYP_CODEGEN_FRIENDLY_NAMES 1

using namespace JSON;

static const HashMap<String, ClassDefinitionType> s_classDefinitionTypes = {
    { "HYP_CLASS", ClassDefinitionType::Class },
    { "HYP_STRUCT", ClassDefinitionType::Struct },
    { "HYP_ENUM", ClassDefinitionType::Enum }
};

static const HashMap<String, MemberType> s_memberDefinitionTypes = {
    { "HYP_FIELD", MemberType::Field },
    { "HYP_METHOD", MemberType::Method },
    { "HYP_PROPERTY", MemberType::Property }
};

// for each path below, if it matches the module path, add the corresponding define(s) to the class condition
static const HashMap<String, String> s_pathConditionalDefines = {
    // platforms
    { "platform/win32", "HYP_WINDOWS" },
    { "platform/linux", "HYP_LINUX" },
    { "platform/mac", "HYP_MAC" },
    { "platform/ios", "HYP_IOS" },
    { "platform/android", "HYP_ANDROID" },
    // rendering backends
    { "rendering/vulkan", "HYP_VULKAN" },
    { "rendering/dx12", "HYP_DX12" },

    { "editor", "HYP_EDITOR" }
};

static void ExtractConditionAttribute(String& condition, Array<Pair<String, ClassAttributeValue>>& attributes)
{
    // handle editoronly attribute
    auto editorOnlyIt = attributes.FindIf([](const Pair<String, ClassAttributeValue>& pair)
        {
            return pair.first.ToLower() == "editoronly";
        });

    if (editorOnlyIt != attributes.End())
    {
        if (condition.Any())
        {
            condition = HYP_FORMAT("{} && {}", condition, "HYP_EDITOR");
        }
        else
        {
            condition = "HYP_EDITOR";
        }

        attributes.Erase(editorOnlyIt);
    }

    auto conditionIt = attributes.FindIf([](const Pair<String, ClassAttributeValue>& pair)
        {
            return pair.first.ToLower() == "condition";
        });

    if (conditionIt != attributes.End())
    {
        if (condition.Any())
        {
            if (conditionIt->second.GetString().Contains("&&") || conditionIt->second.GetString().Contains("||"))
            {
                condition = HYP_FORMAT("{} && ({})", condition, conditionIt->second.GetString());
            }
            else
            {
                condition = HYP_FORMAT("{} && {}", condition, conditionIt->second.GetString());
            }
        }
        else
        {
            condition = conditionIt->second.GetString();
        }

        attributes.Erase(conditionIt);
    }
}

const String& ClassDefinitionTypeToString(ClassDefinitionType type)
{
    auto it = s_classDefinitionTypes.FindIf([type](const Pair<String, ClassDefinitionType>& pair)
        {
            return pair.second == type;
        });

    if (it != s_classDefinitionTypes.End())
    {
        return it->first;
    }

    return String::empty;
}

static void ParseInnerContent(const String& content, String& outResult)
{
    int isInComment = 0; // 0 = no comment, 1 = line comment, 2 = block comment
    bool isInString = false;
    bool isEscaped = false;
    int braceDepth = 0;
    int parenDepth = 0;

    UTF8StringView sv = content;

    for (auto it = sv.Begin(); it != sv.End(); ++it)
    {
        const utf::Char32 ch = *it;

        if (ch == 0)
        {
            break;
        }

        outResult.Append(ch);

        if (isEscaped)
        {
            isEscaped = false;

            continue;
        }

        if (ch == '\\')
        {
            isEscaped = true;
        }
        else if (ch == '\n' && isInComment == 1)
        {
            isInComment = 0;
        }
        else if (ch == '"' && !isInComment)
        {
            isInString = !isInString;
        }
        else if (ch == '/' && !isInString && !isInComment && it + 1 < content.End())
        {
            if (*(it + 1) == '/')
            {
                isInComment = 1;
                outResult.Append(*(++it)); // Append the '/' to the result
                continue;
            }
            else if (*(it + 1) == '*')
            {
                isInComment = 2;
                outResult.Append(*(++it)); // Append the '/' to the result
                continue;
            }
        }
        else if (ch == '*' && !isInString && isInComment == 2 && it + 1 < content.End())
        {
            if (*(it + 1) == '/')
            {
                isInComment = 0;
                outResult.Append(*(++it)); // Append the '/' to the result
                continue;
            }
        }
        else if (!isInString && !isInComment)
        {
            if (ch == '{')
            {
                braceDepth++;
            }
            else if (ch == '}')
            {
                braceDepth--;

                if (braceDepth <= 0 && parenDepth <= 0)
                {
                    break;
                }
            }
            else if (ch == '(')
            {
                parenDepth++;
            }
            else if (ch == ')')
            {
                parenDepth--;
            }
            else if (ch == ';' && braceDepth <= 0)
            {
                break;
            }
        }
    }
}

const String& MemberTypeToString(MemberType type)
{
    auto it = s_memberDefinitionTypes.FindIf([type](const Pair<String, MemberType>& pair)
        {
            return pair.second == type;
        });

    if (it != s_memberDefinitionTypes.End())
    {
        return it->first;
    }

    return String::empty;
}

static TResult<Array<Pair<String, ClassAttributeValue>>> BuildClassAttributes(const String& attributesString)
{
    Array<Pair<String, ClassAttributeValue>> results;

    Array<String> attributes;

    {
        String currentString;
        char previousChar = 0;
        bool inString = false;

        for (char ch : attributesString)
        {
            if (ch == '"' && previousChar != '\\')
            {
                inString = !inString;
            }

            if (ch == ',' && !inString)
            {
                currentString = currentString.Trimmed();

                if (currentString.Any())
                {
                    attributes.PushBack(currentString);
                    currentString.Clear();
                }
            }
            else
            {
                currentString.Append(ch);
            }

            previousChar = ch;
        }

        currentString = currentString.Trimmed();

        if (currentString.Any())
        {
            attributes.PushBack(currentString);
        }
    }

    for (const String& attribute : attributes)
    {
        const size_t equalsIndex = attribute.FindFirstIndex('=');

        if (equalsIndex == String::NotFound)
        {
            // No equals sign, so it's a boolean attribute (true)
            results.PushBack(Pair<String, ClassAttributeValue> { attribute, ClassAttributeValue(true) });

            continue;
        }

        const String key = String(attribute.Substr(0, equalsIndex)).Trimmed();
        const String value = String(attribute.Substr(equalsIndex + 1)).Trimmed();

        if (key.Empty() || value.Empty())
        {
            return HYP_MAKE_ERROR(Error, "Empty key or value in Class attribute");
        }

        ClassAttributeType classAttributeValueType = ClassAttributeType::NONE;
        String classAttributeValueString;

        bool isInString = false;

        bool foundEscape = false;
        bool inQuotes = false;
        bool hasDecimal = false;
        bool isNumeric = false;

        for (size_t i = 0; i < value.Size(); i++)
        {
            const char c = value[i];

            if (c == '"' && !foundEscape)
            {
                inQuotes = !inQuotes;

                classAttributeValueType = ClassAttributeType::STRING;

                continue;
            }

            if (foundEscape)
            {
                foundEscape = false;
            }

            if (c == '\\')
            {
                foundEscape = true;
                continue;
            }

            if (std::isdigit(c) && !inQuotes)
            {
                if (!isNumeric)
                {
                    isNumeric = true;
                    classAttributeValueType = ClassAttributeType::INT;
                }
            }

            classAttributeValueString.Append(c);
        }

        if (classAttributeValueType == ClassAttributeType::NONE)
        {
            const String lower = classAttributeValueString.ToLower();

            if (lower == "true" || lower == "false")
            {
                classAttributeValueType = ClassAttributeType::BOOLEAN;

                classAttributeValueString = lower;
            }
            else
            {
                // Fallback to string
                classAttributeValueType = ClassAttributeType::STRING;
            }
        }

        switch (classAttributeValueType)
        {
        case ClassAttributeType::STRING:
            results.PushBack(Pair<String, ClassAttributeValue> { key, ClassAttributeValue(classAttributeValueString) });
            break;

        case ClassAttributeType::INT:
        {
            int valueInt;

            if (!StringUtil::Parse(classAttributeValueString, &valueInt))
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse int in Class attribute");
            }

            results.PushBack(Pair<String, ClassAttributeValue> { key, ClassAttributeValue(valueInt) });
            break;
        }
        case ClassAttributeType::BOOLEAN:
        {
            bool valueBool;

            if (classAttributeValueString == "true")
            {
                valueBool = true;
            }
            else if (classAttributeValueString == "false")
            {
                valueBool = false;
            }
            else
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse boolean in Class attribute");
            }

            results.PushBack(Pair<String, ClassAttributeValue> { key, ClassAttributeValue(valueBool) });

            break;
        }
        default:
            HYP_UNREACHABLE();
        }
    }

    return results;
}

template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
static TResult<Pair<E, Array<Pair<String, ClassAttributeValue>>>> ParseHypMacro(
    const HashMap<String, E>& usableMacros,
    const String& line,
    size_t& outStartIndex,
    size_t& outEndIndex,
    bool requireParens = true)
{
    outStartIndex = String::NotFound;
    outEndIndex = String::NotFound;

    for (const Pair<String, E>& it : usableMacros)
    {
        size_t macroStartIndex = line.FindFirstIndex(it.first);

        if (macroStartIndex != String::NotFound)
        {
            Array<Pair<String, ClassAttributeValue>> attributes;

            outStartIndex = macroStartIndex;
            outEndIndex = outStartIndex + it.first.Length();

            int parenIndex = -1;

            // skip whitespace after macro name
            for (size_t i = outEndIndex; i < line.Length(); i++)
            {
                const utf::Char32 ch = line.GetChar(i);
                if (ch == ' ' || ch == '\t')
                {
                    outEndIndex++;
                }
                else if (ch == '(')
                {
                    parenIndex = int(i - outEndIndex);
                    break;
                }
                else
                {
                    break;
                }
            }

            if (parenIndex == -1)
            {
                if (requireParens)
                {
                    // Must have parenthesis to be considered an invocation
                    break;
                }

                // Otherwise, empty attributes are used
            }
            else
            {
                outEndIndex = outEndIndex + parenIndex + 1;

                int parenDepth = 1;
                String attributesString;

                for (; outEndIndex < line.Length(); outEndIndex++)
                {
                    const utf::Char32 ch = line.GetChar(outEndIndex);
                    if (ch == '(')
                    {
                        parenDepth++;
                    }
                    else if (ch == ')')
                    {
                        parenDepth--;

                        if (parenDepth <= 0)
                        {
                            outEndIndex++; // Include the closing parenthesis
                            break;
                        }
                    }
                    else
                    {
                        attributesString.Append(ch);
                    }
                }

                auto buildAttributesResult = BuildClassAttributes(attributesString);

                if (buildAttributesResult.HasError())
                {
                    return buildAttributesResult.GetError();
                }

                attributes = buildAttributesResult.GetValue();
            }

            return Pair<E, Array<Pair<String, ClassAttributeValue>>> { it.second, attributes };
        }
    }

    return Pair<E, Array<Pair<String, ClassAttributeValue>>> { E::None, {} };
}

static TResult<Array<ClassDefinition>, AnalyzerError> BuildClasses(const Analyzer& analyzer, Module& mod)
{
    if (!mod.GetPath().Exists())
    {
        HYP_LOG(Tool, Error, "Module path does not exist: {}", mod.GetPath());

        return HYP_MAKE_ERROR(AnalyzerError, "Module path does not exist", mod.GetPath());
    }

    FileBufferedReaderSource source { mod.GetPath() };
    BufferedReader reader { &source };

    if (!reader.IsOpen())
    {
        HYP_LOG(Tool, Error, "Failed to open module file: {}", mod.GetPath());

        return HYP_MAKE_ERROR(AnalyzerError, "Failed to open module file", mod.GetPath());
    }

    Array<ClassDefinition> classDefinitions;

    Array<String> lines = reader.ReadAllLines();

    for (size_t i = 0; i < lines.Size(); i++)
    {
        ClassDefinition classDefinition;

        size_t macroStartIndex;
        size_t macroEndIndex;

        auto parseMacroResult = ParseHypMacro(s_classDefinitionTypes, lines[i], macroStartIndex, macroEndIndex, true);

        if (parseMacroResult.HasError())
        {
            return AnalyzerError(parseMacroResult.GetError(), mod.GetPath());
        }

        if (parseMacroResult.GetValue().first == ClassDefinitionType::None)
        {
            // no match; continue
            continue;
        }

        // look back to build the namespace for the class
        classDefinition.namespaceParts = ExtractCXXNamespacePath(String::Join(lines.Slice(0, i + 1), '\n'));

        classDefinition.type = parseMacroResult.GetValue().first;
        classDefinition.attributes = parseMacroResult.GetValue().second;
        classDefinition.staticIndex = -1;

        { // Set up condition for the class
            const String pathSanitized = mod.GetPath().ToLower().ReplaceAll("\\", "/");

            for (const auto& [pathMatch, define] : s_pathConditionalDefines)
            {
                if (pathSanitized.Contains(pathMatch))
                {
                    if (classDefinition.condition.Any())
                    {
                        classDefinition.condition = HYP_FORMAT("{} && {}", classDefinition.condition, define);
                    }
                    else
                    {
                        classDefinition.condition = define;
                    }
                }
            }

            ExtractConditionAttribute(classDefinition.condition, classDefinition.attributes);
        }

        const String contentToEnd = String::Join(lines.Slice(i, lines.Size()), '\n');

        const size_t braceIndex = contentToEnd.FindFirstIndex("{");

        classDefinition.source = contentToEnd.Substr(0, braceIndex);

        Optional<String> optClassName = ExtractCXXClassName(classDefinition.source);
        if (!optClassName.HasValue())
        {
            HYP_LOG(Tool, Error, "Failed to extract class name from source: {}", classDefinition.source);

            return HYP_MAKE_ERROR(AnalyzerError, "Failed to extract class name", mod.GetPath());
        }

        classDefinition.name = *optClassName;
        // check enum class first so we don't parse it as a class due to the class keyword
        classDefinition.isCXXEnumClass = IsCXXEnumClassDecl(classDefinition.source);
        classDefinition.isCXXClass = !classDefinition.isCXXEnumClass && IsCXXClassDecl(classDefinition.source);
        classDefinition.isCXXStruct = !classDefinition.isCXXClass && IsCXXStructDecl(classDefinition.source);
        classDefinition.isCXXEnum = !classDefinition.isCXXClass && !classDefinition.isCXXStruct && (classDefinition.isCXXEnumClass || IsCXXEnumDecl(classDefinition.source));

        Array<String> baseClassNames = ExtractCXXBaseClasses(classDefinition.source);

        for (const String& baseClassName : baseClassNames)
        {
            classDefinition.baseClassNames.PushBack(baseClassName);
        }

        if (braceIndex != String::NotFound)
        {
            const String remainingContent = contentToEnd.Substr(braceIndex, contentToEnd.Size());

            ParseInnerContent(remainingContent, classDefinition.source);
        }

        AssertDebug(classDefinition.isCXXClass || classDefinition.isCXXStruct || classDefinition.isCXXEnum || classDefinition.isCXXEnumClass,
            "ClassDefinition must be a class, struct, enum, or enum class. Got source:\n\t{}", classDefinition.source);

        // Validate that HYP_CLASS has HYP_OBJECT_BODY and HYP_STRUCT has HYP_STRUCT_BODY
        if (classDefinition.type == ClassDefinitionType::Class)
        {
            if (!classDefinition.isCXXEnum && !classDefinition.isCXXEnumClass)
            {
                if (classDefinition.source.FindFirstIndex("HYP_OBJECT_BODY") == String::NotFound)
                {
                    return HYP_MAKE_ERROR(AnalyzerError, "HYP_CLASS '{}' must contain HYP_OBJECT_BODY(...) in its body", mod.GetPath(), 0, classDefinition.name);
                }
            }
        }
        else if (classDefinition.type == ClassDefinitionType::Struct)
        {
            if (!classDefinition.isCXXEnum && !classDefinition.isCXXEnumClass)
            {
                if (classDefinition.source.FindFirstIndex("HYP_STRUCT_BODY") == String::NotFound)
                {
                    return HYP_MAKE_ERROR(AnalyzerError, "HYP_STRUCT '{}' must contain HYP_STRUCT_BODY(...) in its body", mod.GetPath(), 0, classDefinition.name);
                }
            }
        }

        classDefinitions.PushBack(std::move(classDefinition));
    }

    return classDefinitions;
}

// Add attributes to allow the runtime to access metadata on the member
static void AddMetadata(ASTMemberDecl* decl, MemberDef& result)
{
    if (!decl)
    {
        return;
    }

    if (decl->type && decl->type->IsScriptableDelegate())
    {
        result.AddAttribute("ScriptableDelegate", ClassAttributeValue(true));
    }
}

template <class Function>
static TResult<void, AnalyzerError> CreateParser(const Analyzer& analyzer, const Module& mod, const String& source, Function&& function)
{
    if (source.Empty())
    {
        return HYP_MAKE_ERROR(AnalyzerError, "Source code is empty", mod.GetPath());
    }

    SourceFile sourceFile(mod.GetPath().Basename(), source.Size());

    ByteBuffer temp(source.Size(), source.Data());
    sourceFile.ReadIntoBuffer(temp);

    TokenStream tokenStream(TokenStreamInfo { mod.GetPath().Basename() });

    CompilationUnit unit;
    unit.SetPreprocessorDefinitions(analyzer.GetGlobalDefines());

    const auto checkErrors = [&]() -> TResult<void, AnalyzerError>
    {
        String error_message;

        for (size_t index = 0; index < unit.GetErrorList().Size(); index++)
        {
            error_message += String::ToString(unit.GetErrorList()[index].GetLocation().GetLine() + 1)
                + "," + String::ToString(unit.GetErrorList()[index].GetLocation().GetColumn() + 1)
                + ": " + unit.GetErrorList()[index].GetText() + "\n";
        }

        if (unit.GetErrorList().HasFatalErrors())
        {
            return HYP_MAKE_ERROR(AnalyzerError, "Failed to parse member: {}", mod.GetPath(), 0, error_message);
        }

        return {};
    };

    Lexer lexer(SourceStream(&sourceFile), &tokenStream, &unit);
    lexer.Analyze();

    if (auto res = checkErrors(); res.HasError())
    {
        return res.GetError();
    }

    if (tokenStream.Eof())
    {
        return HYP_MAKE_ERROR(AnalyzerError, "Token stream is empty", mod.GetPath());
    }

    Parser parser(&tokenStream, &unit);

    if (auto res = function(parser); res.HasError())
    {
        return res.GetError();
    }

    return checkErrors();
}

static TResult<Array<MemberDef>, AnalyzerError> BuildClassMembers(const Analyzer& analyzer, const Module& mod, const ClassDefinition& classDefinition)
{
    Array<MemberDef> results;

    Array<String> lines = classDefinition.source.Split('\n');

    for (size_t i = 0; i < lines.Size(); i++)
    {
        const String& line = lines[i];

        size_t macroStartIndex;
        size_t macroEndIndex;

        auto parseMacroResult = ParseHypMacro(s_memberDefinitionTypes, line, macroStartIndex, macroEndIndex, false);

        if (parseMacroResult.HasError())
        {
            return AnalyzerError(parseMacroResult.GetError(), mod.GetPath());
        }

        if (parseMacroResult.GetValue().first == MemberType::None)
        {
            continue;
        }

        MemberDef& result = results.EmplaceBack();
        result.type = parseMacroResult.GetValue().first;
        result.attributes = parseMacroResult.GetValue().second;

        if (result.type == MemberType::Property)
        {
            if (result.attributes.Empty() || result.attributes[0].first.Empty())
            {
                return HYP_MAKE_ERROR(AnalyzerError, "Property must have a name", mod.GetPath());
            }

            result.name = result.attributes.PopFront().first;

            ExtractConditionAttribute(result.condition, result.attributes);

            continue;
        }

        const String contentToEnd = String(line.Substr(macroEndIndex)) + "\n" + String::Join(lines.Slice(i + 1, lines.Size()), '\n');
        ParseInnerContent(contentToEnd, result.source);

        RC<ASTMemberDecl> decl;

        auto res = CreateParser(analyzer, mod, result.source, [&](Parser& parser) -> TResult<void, AnalyzerError>
            {
                decl = parser.ParseMemberDecl();

                return {};
            });

        if (res.HasError())
        {
            return res.GetError();
        }

        Assert(decl != nullptr);
        AddMetadata(decl, result);

        result.name = decl->name;
        result.cxxType = decl->type;
        result.cxxDecl = decl;

        ExtractConditionAttribute(result.condition, result.attributes);
    }

    return results;
}

static TResult<Array<MemberDef>, AnalyzerError> BuildEnumMembers(const Analyzer& analyzer, const Module& mod, const ClassDefinition& classDefinition)
{
    Array<MemberDef> results;

    String innerContent;
    ParseInnerContent(classDefinition.source, innerContent);

    size_t openingBraceIndex = innerContent.FindFirstIndex('{');

    if (openingBraceIndex == String::NotFound)
    {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to find opening brace for enum", mod.GetPath());
    }

    // Extract the content inside the braces
    innerContent = innerContent.Substr(openingBraceIndex + 1);

    // Find the closing brace
    size_t closingBraceIndex = innerContent.FindLastIndex('}');

    if (closingBraceIndex == String::NotFound)
    {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to find closing brace for enum", mod.GetPath());
    }
    // Extract the content inside the braces
    innerContent = innerContent.Substr(0, closingBraceIndex);

    auto res = CreateParser(analyzer, mod, innerContent, [&](Parser& parser) -> TResult<void, AnalyzerError>
        {
            RC<ASTMemberDecl> memberDecl;

            uint32 member_index = 0;

            do
            {
                MemberDef memberDef;
                memberDef.type = MemberType::StaticField;

                memberDecl = parser.ParseEnumMemberDecl(nullptr);

                if (!memberDecl)
                {
                    return HYP_MAKE_ERROR(AnalyzerError, "Failed to parse enum member declaration", mod.GetPath());
                }

                memberDef.name = memberDecl->name;
                memberDef.cxxType = memberDecl->type;
                memberDef.cxxDecl = memberDecl;

                if (memberDef.name.Empty())
                {
                    return HYP_MAKE_ERROR(AnalyzerError, "Enum member must have a name for element at index {}", mod.GetPath(), 0, member_index);
                }

                // Add the member to the results
                results.PushBack(std::move(memberDef));

                ++member_index;
            }
            while (parser.Match(TK_COMMA, true));

            return {};
        });

    if (res.HasError())
    {
        return res.GetError();
    }

    return results;
}

#pragma region Analyzer

Analyzer::Analyzer()
{
    // clang-format off

    // reserve 'ObjectBase' class
    ClassDefinition& classDefinition = m_builtinClasses.Emplace("ObjectBase", ClassDefinition { }).first->second;
    classDefinition.type = ClassDefinitionType::Class;
    classDefinition.name = "ObjectBase";
    classDefinition.staticIndex = 0;
    classDefinition.isCXXClass = true;
    classDefinition.namespaceParts = Array<String> { BaseNamespace };

    // clang-format on
}

const ClassDefinition* Analyzer::FindClassDefinition(UTF8StringView className) const
{
    Mutex::Guard guard(m_mutex);

    return FindClassDefinition_Internal(className);
}

const ClassDefinition* Analyzer::FindClassDefinition_Internal(UTF8StringView className) const
{
    auto it = m_builtinClasses.FindAs(className);

    if (it != m_builtinClasses.End())
    {
        return &it->second;
    }

    for (const UniquePtr<Module>& mod : m_modules)
    {
        const ClassDefinition* classDefinition = mod->FindClassDefinition(className);

        if (classDefinition)
        {
            return classDefinition;
        }
    }

    return nullptr;
}

Module* Analyzer::AddModule(const FilePath& path)
{
    Mutex::Guard guard(m_mutex);

    return m_modules.PushBack(MakeUnique<Module>(path)).Get();
}

TResult<void, AnalyzerError> Analyzer::ProcessModule(Module& mod)
{
    auto res = BuildClasses(*this, mod);

    if (res.HasError())
    {
        return res.GetError();
    }

    for (ClassDefinition& classDefinition : res.GetValue())
    {
        TResult<Array<MemberDef>, AnalyzerError> res = Array<MemberDef> {};

        switch (classDefinition.type)
        {
        case ClassDefinitionType::Class:
        case ClassDefinitionType::Struct: // fallthrough
            res = BuildClassMembers(*this, mod, classDefinition);

            break;
        case ClassDefinitionType::Enum:
            res = BuildEnumMembers(*this, mod, classDefinition);

            break;
        default:
            return HYP_MAKE_ERROR(AnalyzerError, "Unknown ClassDefinitionType", mod.GetPath());
        }

        if (res.HasError())
        {
            HYP_LOG(Tool, Error, "Failed to build class definition: {}\tError code: {}", res.GetError().GetMessage(), res.GetError().GetErrorCode());

            return res.GetError();
        }

        Array<MemberDef> members = std::move(res.GetValue());

        for (MemberDef& definition : members)
        {
            definition.friendlyName = definition.name;

            switch (definition.type)
            {
            case MemberType::StaticField: // fallthrough
            case MemberType::Field:       // fallthrough
            case MemberType::Property:
            {
#if HYP_CODEGEN_FRIENDLY_NAMES
                bool preserveCase = true;

                if (classDefinition.type == ClassDefinitionType::Enum)
                {
                    break; // don't change enum member names
                }

                if (definition.cxxType != nullptr
                    && definition.cxxType->isStatic
                    && (definition.cxxType->isConst || definition.cxxType->isConstexpr))
                {
                    // static const / constexpr members could be in ALL_CAPS case, although we generally don't use that style
                    preserveCase = false;
                }

                String nameWithoutPrefix = definition.name;

                if (nameWithoutPrefix.StartsWith("m_") || nameWithoutPrefix.StartsWith("s_") || nameWithoutPrefix.StartsWith("g_"))
                {
                    nameWithoutPrefix = nameWithoutPrefix.Substr(2);
                }

                definition.friendlyName = StringUtil::ToPascalCase(nameWithoutPrefix, preserveCase);
#endif

                break;
            }
                // Methods need no change
            case MemberType::Method: // fallthrough
            default:
                break;
            }
        }

        classDefinition.members = std::move(members);

        AssertDebug(classDefinition.isCXXClass || classDefinition.isCXXStruct || classDefinition.isCXXEnumClass || classDefinition.isCXXEnum,
            "ClassDefinition must be a C++ class, struct, or enum");

        mod.AddClassDefinition(std::move(classDefinition));
    }

    return {};
}

bool Analyzer::HasBaseClass(const ClassDefinition& classDefinition, UTF8StringView baseClassName) const
{
    Mutex::Guard guard(m_mutex);

    Proc<bool(const ClassDefinition&, UTF8StringView)> performCheck;

    performCheck = [this, &performCheck](const ClassDefinition& classDefinition, UTF8StringView className) -> bool
    {
        auto it = classDefinition.baseClassNames.FindAs(className);

        if (it != classDefinition.baseClassNames.End())
        {
            return true;
        }

        for (const String& baseClassName : classDefinition.baseClassNames)
        {
            const ClassDefinition* baseClass = FindClassDefinition_Internal(baseClassName);

            if (baseClass && performCheck(*baseClass, className))
            {
                return true;
            }
        }

        return false;
    };

    return performCheck(classDefinition, baseClassName);
}

#pragma endregion Analyzer

} // namespace CodeGen
} // namespace Hyperion