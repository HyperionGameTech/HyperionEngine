/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Lexer.hpp>
#include <parser/Parser.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/StringView.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/Forest.hpp>

#include <core/functional/Proc.hpp>

#include <core/logging/Logger.hpp>

#include <core/math/MathUtil.hpp>

#include <core/io/BufferedByteReader.hpp>

#include <util/ParseUtil.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {
namespace buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

using namespace json;

static const HashMap<String, HypClassDefinitionType> g_hypClassDefinitionTypes = {
    { "HYP_CLASS", HypClassDefinitionType::CLASS },
    { "HYP_STRUCT", HypClassDefinitionType::STRUCT },
    { "HYP_ENUM", HypClassDefinitionType::ENUM }
};

static const HashMap<String, HypMemberType> g_hypMemberDefinitionTypes = {
    { "HYP_FIELD", HypMemberType::TYPE_FIELD },
    { "HYP_METHOD", HypMemberType::TYPE_METHOD },
    { "HYP_PROPERTY", HypMemberType::TYPE_PROPERTY }
};

const String& HypClassDefinitionTypeToString(HypClassDefinitionType type)
{
    auto it = g_hypClassDefinitionTypes.FindIf([type](const Pair<String, HypClassDefinitionType>& pair)
        {
            return pair.second == type;
        });

    if (it != g_hypClassDefinitionTypes.End())
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

    for (SizeType charIndex = 0; charIndex < content.Length(); charIndex++)
    {
        const utf::u32char ch = content.GetChar(charIndex);

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
        else if (ch == '/' && !isInString && !isInComment && charIndex + 1 < content.Length())
        {
            if (content.GetChar(charIndex + 1) == '/')
            {
                isInComment = 1;
                outResult.Append(content.GetChar(++charIndex)); // Append the '/' to the result
                continue;
            }
            else if (content.GetChar(charIndex + 1) == '*')
            {
                isInComment = 2;
                outResult.Append(content.GetChar(++charIndex)); // Append the '/' to the result
                continue;
            }
        }
        else if (ch == '*' && !isInString && isInComment == 2 && charIndex + 1 < content.Length())
        {
            if (content.GetChar(charIndex + 1) == '/')
            {
                isInComment = 0;
                outResult.Append(content.GetChar(++charIndex)); // Append the '/' to the result
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

const String& HypMemberTypeToString(HypMemberType type)
{
    auto it = g_hypMemberDefinitionTypes.FindIf([type](const Pair<String, HypMemberType>& pair)
        {
            return pair.second == type;
        });

    if (it != g_hypMemberDefinitionTypes.End())
    {
        return it->first;
    }

    return String::empty;
}

static TResult<Array<Pair<String, HypClassAttributeValue>>> BuildHypClassAttributes(const String& attributesString)
{
    Array<Pair<String, HypClassAttributeValue>> results;

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
        const SizeType equalsIndex = attribute.FindFirstIndex('=');

        if (equalsIndex == String::notFound)
        {
            // No equals sign, so it's a boolean attribute (true)
            results.PushBack(Pair<String, HypClassAttributeValue> { attribute, HypClassAttributeValue(true) });

            continue;
        }

        const String key = String(attribute.Substr(0, equalsIndex)).Trimmed();
        const String value = String(attribute.Substr(equalsIndex + 1)).Trimmed();

        if (key.Empty() || value.Empty())
        {
            return HYP_MAKE_ERROR(Error, "Empty key or value in HypClass attribute");
        }

        HypClassAttributeType hypClassAttributeValueType = HypClassAttributeType::NONE;
        String hypClassAttributeValueString;

        bool isInString = false;

        bool foundEscape = false;
        bool inQuotes = false;
        bool hasDecimal = false;
        bool isNumeric = false;

        for (SizeType i = 0; i < value.Size(); i++)
        {
            const char c = value[i];

            if (c == '"' && !foundEscape)
            {
                inQuotes = !inQuotes;

                hypClassAttributeValueType = HypClassAttributeType::STRING;

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

            if (isdigit(c) && !inQuotes)
            {
                if (!isNumeric)
                {
                    isNumeric = true;
                    hypClassAttributeValueType = HypClassAttributeType::INT;
                }
            }
            else if (c == '.' && !inQuotes && isNumeric && hypClassAttributeValueType == HypClassAttributeType::INT)
            {
                hypClassAttributeValueType = HypClassAttributeType::FLOAT;
                hasDecimal = true;
            }

            hypClassAttributeValueString.Append(c);
        }

        if (hypClassAttributeValueType == HypClassAttributeType::NONE)
        {
            const String lower = hypClassAttributeValueString.ToLower();

            if (lower == "true" || lower == "false")
            {
                hypClassAttributeValueType = HypClassAttributeType::BOOLEAN;

                hypClassAttributeValueString = lower;
            }
            else
            {
                // Fallback to string
                hypClassAttributeValueType = HypClassAttributeType::STRING;
            }
        }

        switch (hypClassAttributeValueType)
        {
        case HypClassAttributeType::STRING:
            results.PushBack(Pair<String, HypClassAttributeValue> { key, HypClassAttributeValue(hypClassAttributeValueString) });
            break;

        case HypClassAttributeType::INT:
        {
            int int_value;

            if (!StringUtil::Parse<int>(hypClassAttributeValueString, &int_value))
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse int in HypClass attribute");
            }

            results.PushBack(Pair<String, HypClassAttributeValue> { key, HypClassAttributeValue(int_value) });
            break;
        }
        case HypClassAttributeType::FLOAT:
        {
            double float_value;

            if (!StringUtil::Parse<double>(hypClassAttributeValueString, &float_value))
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse float in HypClass attribute");
            }

            results.PushBack(Pair<String, HypClassAttributeValue> { key, HypClassAttributeValue(float_value) });
            break;
        }
        case HypClassAttributeType::BOOLEAN:
        {
            bool bool_value;

            if (hypClassAttributeValueString == "true")
            {
                bool_value = true;
            }
            else if (hypClassAttributeValueString == "false")
            {
                bool_value = false;
            }
            else
            {
                return HYP_MAKE_ERROR(Error, "Failed to parse boolean in HypClass attribute");
            }

            results.PushBack(Pair<String, HypClassAttributeValue> { key, HypClassAttributeValue(bool_value) });

            break;
        }
        default:
            HYP_UNREACHABLE();
        }
    }

    return results;
}

template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
static TResult<Pair<E, Array<Pair<String, HypClassAttributeValue>>>> ParseHypMacro(const HashMap<String, E>& usableMacros, const String& line, SizeType& outStartIndex, SizeType& outEndIndex, bool require_parentheses = true)
{
    outStartIndex = String::notFound;
    outEndIndex = String::notFound;

    for (const Pair<String, E>& it : usableMacros)
    {
        SizeType macroStartIndex = line.FindFirstIndex(it.first);

        if (macroStartIndex != String::notFound)
        {
            Array<Pair<String, HypClassAttributeValue>> attributes;

            outStartIndex = macroStartIndex;
            outEndIndex = outStartIndex + it.first.Length();

            const SizeType parenIndex = line.Substr(outEndIndex).FindFirstIndex("(");

            if (parenIndex == String::notFound)
            {
                if (require_parentheses)
                {
                    // Must have parenthesis to be considered an invocation
                    continue;
                }

                // Otherwise, empty attributes are used
            }
            else
            {
                outEndIndex = outEndIndex + parenIndex + 1;

                int parenDepth = 1;
                String attributesString;

                for (; outEndIndex < line.Size(); outEndIndex++)
                {
                    if (line[outEndIndex] == '(')
                    {
                        parenDepth++;
                    }
                    else if (line[outEndIndex] == ')')
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
                        attributesString.Append(line[outEndIndex]);
                    }
                }

                auto buildAttributesResult = BuildHypClassAttributes(attributesString);

                if (buildAttributesResult.HasError())
                {
                    return buildAttributesResult.GetError();
                }

                attributes = buildAttributesResult.GetValue();
            }

            return Pair<E, Array<Pair<String, HypClassAttributeValue>>> { it.second, attributes };
        }
    }

    return Pair<E, Array<Pair<String, HypClassAttributeValue>>> { E::NONE, {} };
}

static TResult<Array<HypClassDefinition>, AnalyzerError> BuildHypClasses(const Analyzer& analyzer, Module& mod)
{
    if (!mod.GetPath().Exists())
    {
        HYP_LOG(BuildTool, Error, "Module path does not exist: {}", mod.GetPath());

        return HYP_MAKE_ERROR(AnalyzerError, "Module path does not exist", mod.GetPath());
    }

    FileBufferedReaderSource source { mod.GetPath() };
    BufferedReader reader { &source };

    if (!reader.IsOpen())
    {
        HYP_LOG(BuildTool, Error, "Failed to open module file: {}", mod.GetPath());

        return HYP_MAKE_ERROR(AnalyzerError, "Failed to open module file", mod.GetPath());
    }

    Array<HypClassDefinition> hypClassDefinitions;

    Array<String> lines = reader.ReadAllLines();

    for (SizeType i = 0; i < lines.Size(); i++)
    {
        HypClassDefinition hypClassDefinition;

        SizeType macroStartIndex;
        SizeType macroEndIndex;

        auto parseMacroResult = ParseHypMacro(g_hypClassDefinitionTypes, lines[i], macroStartIndex, macroEndIndex, true);

        if (parseMacroResult.HasError())
        {
            return AnalyzerError(parseMacroResult.GetError(), mod.GetPath());
        }

        if (parseMacroResult.GetValue().first == HypClassDefinitionType::NONE)
        {
            // no match; continue
            continue;
        }

        hypClassDefinition.type = parseMacroResult.GetValue().first;
        hypClassDefinition.attributes = parseMacroResult.GetValue().second;
        hypClassDefinition.staticIndex = -1;

        const String contentToEnd = String::Join(lines.Slice(i, lines.Size()), '\n');

        const SizeType braceIndex = contentToEnd.FindFirstIndex("{");

        hypClassDefinition.source = contentToEnd.Substr(0, braceIndex);

        Optional<String> optClassName = ExtractCXXClassName(hypClassDefinition.source);
        if (!optClassName.HasValue())
        {
            HYP_LOG(BuildTool, Error, "Failed to extract class name from source: {}", hypClassDefinition.source);

            return HYP_MAKE_ERROR(AnalyzerError, "Failed to extract class name", mod.GetPath());
        }

        hypClassDefinition.name = *optClassName;

        Array<String> baseClassNames = ExtractCXXBaseClasses(hypClassDefinition.source);
        for (const String& baseClassName : baseClassNames)
        {
            hypClassDefinition.baseClassNames.PushBack(baseClassName);
        }

        if (braceIndex != String::notFound)
        {
            const String remainingContent = contentToEnd.Substr(braceIndex, contentToEnd.Size());

            ParseInnerContent(remainingContent, hypClassDefinition.source);
        }

        hypClassDefinitions.PushBack(std::move(hypClassDefinition));
    }

    return hypClassDefinitions;
}

// Add attributes to allow the runtime to access metadata on the member
static void AddMetadata(ASTMemberDecl* decl, HypMemberDefinition& result)
{
    if (!decl)
    {
        return;
    }

    if (decl->type && decl->type->IsScriptableDelegate())
    {
        result.AddAttribute("ScriptableDelegate", HypClassAttributeValue(true));
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

        for (SizeType index = 0; index < unit.GetErrorList().Size(); index++)
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

static TResult<Array<HypMemberDefinition>, AnalyzerError> BuildHypClassMembers(const Analyzer& analyzer, const Module& mod, const HypClassDefinition& hypClassDefinition)
{
    Array<HypMemberDefinition> results;

    Array<String> lines = hypClassDefinition.source.Split('\n');

    for (SizeType i = 0; i < lines.Size(); i++)
    {
        const String& line = lines[i];

        SizeType macroStartIndex;
        SizeType macroEndIndex;

        auto parseMacroResult = ParseHypMacro(g_hypMemberDefinitionTypes, line, macroStartIndex, macroEndIndex, false);

        if (parseMacroResult.HasError())
        {
            return AnalyzerError(parseMacroResult.GetError(), mod.GetPath());
        }

        if (parseMacroResult.GetValue().first == HypMemberType::NONE)
        {
            continue;
        }

        HypMemberDefinition& result = results.EmplaceBack();
        result.type = parseMacroResult.GetValue().first;
        result.attributes = parseMacroResult.GetValue().second;

        if (result.type == HypMemberType::TYPE_PROPERTY)
        {
            if (result.attributes.Empty() || result.attributes[0].first.Empty())
            {
                return HYP_MAKE_ERROR(AnalyzerError, "Property must have a name", mod.GetPath());
            }

            result.name = result.attributes.PopFront().first;

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
    }

    return results;
}

static TResult<Array<HypMemberDefinition>, AnalyzerError> BuildHypEnumMembers(const Analyzer& analyzer, const Module& mod, const HypClassDefinition& hypClassDefinition)
{
    Array<HypMemberDefinition> results;

    String innerContent;
    ParseInnerContent(hypClassDefinition.source, innerContent);

    SizeType openingBraceIndex = innerContent.FindFirstIndex('{');

    if (openingBraceIndex == String::notFound)
    {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to find opening brace for enum", mod.GetPath());
    }

    // Extract the content inside the braces
    innerContent = innerContent.Substr(openingBraceIndex + 1);

    // Find the closing brace
    SizeType closingBraceIndex = innerContent.FindLastIndex('}');

    if (closingBraceIndex == String::notFound)
    {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to find closing brace for enum", mod.GetPath());
    }
    // Extract the content inside the braces
    innerContent = innerContent.Substr(0, closingBraceIndex);

    auto res = CreateParser(analyzer, mod, innerContent, [&](Parser& parser) -> TResult<void, AnalyzerError>
        {
            RC<ASTMemberDecl> member_decl;

            uint32 member_index = 0;

            do
            {
                HypMemberDefinition hyp_member_definition;
                hyp_member_definition.type = HypMemberType::TYPE_CONSTANT;

                member_decl = parser.ParseEnumMemberDecl(nullptr);

                if (!member_decl)
                {
                    return HYP_MAKE_ERROR(AnalyzerError, "Failed to parse enum member declaration", mod.GetPath());
                }

                hyp_member_definition.name = member_decl->name;
                hyp_member_definition.cxxType = member_decl->type;

                if (hyp_member_definition.name.Empty())
                {
                    return HYP_MAKE_ERROR(AnalyzerError, "Enum member must have a name for element at index {}", mod.GetPath(), 0, member_index);
                }

                // Add the member to the results
                results.PushBack(std::move(hyp_member_definition));

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

const HypClassDefinition* Analyzer::FindHypClassDefinition(UTF8StringView class_name) const
{
    Mutex::Guard guard(m_mutex);

    for (const UniquePtr<Module>& module : m_modules)
    {
        const HypClassDefinition* hyp_class = module->FindHypClassDefinition(class_name);

        if (hyp_class)
        {
            return hyp_class;
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
    auto res = BuildHypClasses(*this, mod);

    if (res.HasError())
    {
        return res.GetError();
    }

    for (HypClassDefinition& hypClassDefinition : res.GetValue())
    {
        TResult<Array<HypMemberDefinition>, AnalyzerError> res = Array<HypMemberDefinition> {};

        HYP_LOG(BuildTool, Info, "Building class definition: {} ({})", hypClassDefinition.name, HypClassDefinitionTypeToString(hypClassDefinition.type));

        switch (hypClassDefinition.type)
        {
        case HypClassDefinitionType::CLASS:
        case HypClassDefinitionType::STRUCT: // fallthrough
            res = BuildHypClassMembers(*this, mod, hypClassDefinition);

            break;
        case HypClassDefinitionType::ENUM:
            res = BuildHypEnumMembers(*this, mod, hypClassDefinition);

            break;
        }

        if (res.HasError())
        {
            HYP_LOG(BuildTool, Error, "Failed to build class definition: {}\tError code: {}", res.GetError().GetMessage(), res.GetError().GetErrorCode());

            return res.GetError();
        }

        Array<HypMemberDefinition> members = std::move(res.GetValue());

        for (HypMemberDefinition& definition : members)
        {
            switch (definition.type)
            {
            case HypMemberType::TYPE_CONSTANT:
            case HypMemberType::TYPE_FIELD: // fallthrough
            {
                bool preserveCase = true;

                if (hypClassDefinition.type == HypClassDefinitionType::ENUM)
                {
                    // ensure ALL_CAPS enum members get converted to PascalCase. don't preserve their casing.
                    preserveCase = false;
                }
                else if (definition.cxxType->isStatic && (definition.cxxType->isConst || definition.cxxType->isConstexpr))
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

                break;
            }
            default:
                definition.friendlyName = definition.name;
                break;
            }
        }

        hypClassDefinition.members = std::move(members);

        mod.AddHypClassDefinition(std::move(hypClassDefinition));
    }

    return {};
}

bool Analyzer::HasBaseClass(const HypClassDefinition& hypClassDefinition, UTF8StringView baseClassName) const
{
    Mutex::Guard guard(m_mutex);

    auto findDefinition = [this](UTF8StringView name) -> const HypClassDefinition*
    {
        for (const UniquePtr<Module>& module : m_modules)
        {
            const HypClassDefinition* hyp_class = module->FindHypClassDefinition(name);

            if (hyp_class)
            {
                return hyp_class;
            }
        }

        return nullptr;
    };

    Proc<bool(const HypClassDefinition&, UTF8StringView)> performCheck;

    performCheck = [&performCheck, &findDefinition](const HypClassDefinition& hypClassDefinition, UTF8StringView baseClassName) -> bool
    {
        auto it = hypClassDefinition.baseClassNames.FindAs(baseClassName);

        if (it != hypClassDefinition.baseClassNames.End())
        {
            return true;
        }

        for (const String& baseClass : hypClassDefinition.baseClassNames)
        {
            const HypClassDefinition* baseHypClass = findDefinition(baseClass);

            if (baseHypClass && performCheck(*baseHypClass, baseClassName))
            {
                return true;
            }
        }

        return false;
    };

    return performCheck(hypClassDefinition, baseClassName);
}

#pragma endregion Analyzer

} // namespace buildtool
} // namespace hyperion