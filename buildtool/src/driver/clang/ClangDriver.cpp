/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <driver/clang/ClangDriver.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Lexer.hpp>
#include <parser/Parser.hpp>

#include <core/utilities/Format.hpp>
#include <core/utilities/StringView.hpp>

#include <core/containers/HashMap.hpp>

#include <core/logging/Logger.hpp>

#include <math/MathUtil.hpp>

#include <asset/BufferedByteReader.hpp>

#include <util/ParseUtil.hpp>

#include <core/json/JSON.hpp>

#include <stdio.h>
#include <unistd.h>

namespace hyperion {
namespace buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);

using namespace json;

class TmpFile
{
public:
    TmpFile(const UTF8StringView &suffix)
        : m_path(String(std::tmpnam(nullptr)) + suffix),
          m_file(fopen(m_path.Data(), "w+"))
    {
        if (!m_file) {
            HYP_LOG(BuildTool, Error, "Failed to create temporary file: {}", m_path);

            m_path.Clear();
        }
    }

    TmpFile(const TmpFile &)            = delete;
    TmpFile &operator=(const TmpFile &) = delete;

    TmpFile(TmpFile &&other)
        : m_path(std::move(other.m_path)),
          m_file(other.m_file)
    {
        other.m_file = nullptr;
    }

    TmpFile &operator=(TmpFile &&other)
    {
        if (this != &other) {
            if (m_file) {
                fclose(m_file);
            }

            m_path = std::move(other.m_path);

            m_file = other.m_file;
            other.m_file = nullptr;
        }

        return *this;
    }

    ~TmpFile()
    {
        Close();
    }

    HYP_FORCE_INLINE const FilePath &GetPath() const
        { return m_path; }

    HYP_FORCE_INLINE FILE *GetFile() const
        { return m_file; }

    TmpFile &operator<<(UTF8StringView str)
    {
        if (m_file) {
            fwrite(str.Data(), 1, str.Size(), m_file);
        }

        return *this;
    }

    void Close()
    {
        if (m_file) {
            fclose(m_file);

            m_file = nullptr;
        }

        if (m_path.Any()) {
            remove(m_path.Data());

            m_path.Clear();
        }
    }

    void Flush()
    {
        if (m_file) {
            fflush(m_file);
        }
    }

private:
    FilePath    m_path;
    FILE        *m_file;
};


static Result<JSONValue> ParseCXXHeader(const String &header, const HashMap<String, String> &defines = { })
{
    String defines_string;

    for (const Pair<String, String> &define : defines) {
        defines_string += HYP_FORMAT(" -D'{}={}'", define.first, define.second);
    }

    TmpFile tmp_file { ".hpp" };
    tmp_file << header << "\n";
    tmp_file.Flush();

    String command = HYP_FORMAT("clang -std=c++20 -Xclang -ast-dump=json -fsyntax-only {} {}",
        defines_string, tmp_file.GetPath());

    // HYP_LOG(BuildTool, Info, "Output:\n\n{}", class_content);
    HYP_LOG(BuildTool, Info, "Running clang command: {}", command);
        
    ByteBuffer buffer;
    buffer.SetSize(4096);

    FILE *file = popen(command.Data(), "r");

    if (!file) {
        return HYP_MAKE_ERROR(Error, "Failed to run clang");
    }

    SizeType buffer_offset = 0;

    while (!feof(file)) {
        const SizeType offset = buffer_offset;
        buffer.SetSize(buffer.Size() * 2);

        SizeType num_read_bytes = fread(buffer.Data() + offset, 1, buffer.Size() - offset, file);

        if (num_read_bytes == 0) {
            break;
        }

        buffer_offset += num_read_bytes;
    }

    buffer.SetSize(buffer_offset);

    pclose(file);

    BufferedReader reader(MakeRefCountedPtr<MemoryBufferedReaderSource>(buffer.ToByteView()));
    
    if (reader.Eof()) {
        return HYP_MAKE_ERROR(Error, "Failed to read output");
    }

    auto json_parse_result = JSON::Parse(reader);

    if (!json_parse_result.ok) {
        return HYP_MAKE_ERROR(Error, "Failed to parse JSON output");
    }

    HYP_LOG(BuildTool, Info, "Parsed JSON: {}", json_parse_result.value.ToString());

    return json_parse_result.value;
}

static const HashMap<String, HypClassDefinitionType> g_hyp_class_definition_types = {
    { "HYP_CLASS", HypClassDefinitionType::CLASS },
    { "HYP_STRUCT", HypClassDefinitionType::STRUCT },
    { "HYP_ENUM", HypClassDefinitionType::ENUM }
};

static const String &HypClassDefinitionTypeToString(HypClassDefinitionType type)
{
    auto it = g_hyp_class_definition_types.FindIf([type](const Pair<String, HypClassDefinitionType> &pair)
    {
        return pair.second == type;
    });

    if (it != g_hyp_class_definition_types.End()) {
        return it->first;
    }

    return String::empty;
}

static const HashMap<String, HypMemberType> g_hyp_member_definition_types = {
    { "HYP_FIELD", HypMemberType::TYPE_FIELD },
    { "HYP_METHOD", HypMemberType::TYPE_METHOD },
    { "HYP_PROPERTY", HypMemberType::TYPE_PROPERTY },
    { "HYP_CONSTANT", HypMemberType::TYPE_CONSTANT }
};

static const String &HypMemberTypeToString(HypMemberType type)
{
    auto it = g_hyp_member_definition_types.FindIf([type](const Pair<String, HypMemberType> &pair)
    {
        return pair.second == type;
    });

    if (it != g_hyp_member_definition_types.End()) {
        return it->first;
    }

    return String::empty;
}

static Result<Array<Pair<String, HypClassAttributeValue>>> BuildHypClassAttributes(const String &attributes_string)
{
    Array<Pair<String, HypClassAttributeValue>> results;

    Array<String> attributes;

    {
        String current_string;
        char previous_char = 0;
        bool in_string = false;

        for (char ch : attributes_string) {
            if (ch == '"' && previous_char != '\\') {
                in_string = !in_string;
            }

            if (ch == ',' && !in_string) {
                current_string = current_string.Trimmed();

                if (current_string.Any()) {
                    attributes.PushBack(current_string);
                    current_string.Clear();
                }
            } else {
                current_string.Append(ch);
            }

            previous_char = ch;
        }

        current_string = current_string.Trimmed();

        if (current_string.Any()) {
            attributes.PushBack(current_string);
        }
    }

    for (const String &attribute : attributes) {
        const SizeType equals_index = attribute.FindFirstIndex('=');

        if (equals_index == String::not_found) {
            // No equals sign, so it's a boolean attribute (true)
            results.PushBack(Pair<String, HypClassAttributeValue> { attribute, HypClassAttributeValue(true) });

            continue;
        }

        const String key = String(attribute.Substr(0, equals_index)).Trimmed();
        const String value = String(attribute.Substr(equals_index + 1)).Trimmed();

        if (key.Empty() || value.Empty()) {
            return HYP_MAKE_ERROR(Error, "Empty key or value in HypClass attribute");
        }

        HypClassAttributeType hyp_class_attribute_value_type = HypClassAttributeType::NONE;
        String hyp_class_attribute_value_string;
        
        bool is_in_string = false;
        
        bool found_escape = false;
        bool in_quotes = false;
        bool has_decimal = false;
        bool is_numeric = false;

        for (SizeType i = 0; i < value.Size(); i++) {
            const char c = value[i];

            if (c == '"' && !found_escape) {
                in_quotes = !in_quotes;

                hyp_class_attribute_value_type = HypClassAttributeType::STRING;

                continue;
            }
            
            if (found_escape) {
                found_escape = false;
            }
            
            if (c == '\\') {
                found_escape = true;
                continue;
            }
            
            if (isdigit(c) && !in_quotes) {
                if (!is_numeric) {
                    is_numeric = true;
                    hyp_class_attribute_value_type = HypClassAttributeType::INT;
                }
            } else if (c == '.' && !in_quotes && is_numeric && hyp_class_attribute_value_type == HypClassAttributeType::INT) {
                hyp_class_attribute_value_type = HypClassAttributeType::FLOAT;
                has_decimal = true;
            }

            hyp_class_attribute_value_string.Append(c);
        }

        if (hyp_class_attribute_value_type == HypClassAttributeType::NONE) {
            const String lower = hyp_class_attribute_value_string.ToLower();

            if (lower == "true" || lower == "false") {
                hyp_class_attribute_value_type = HypClassAttributeType::BOOLEAN;

                hyp_class_attribute_value_string = lower;
            } else {
                // Fallback to string
                hyp_class_attribute_value_type = HypClassAttributeType::STRING;
            }
        }

        switch (hyp_class_attribute_value_type) {
        case HypClassAttributeType::STRING:
            results.PushBack(Pair<String, HypClassAttributeValue> { key, HypClassAttributeValue(hyp_class_attribute_value_string) });
            break;

        case HypClassAttributeType::INT: {
            int int_value;

            if (!StringUtil::Parse<int>(hyp_class_attribute_value_string, &int_value)) {
                return HYP_MAKE_ERROR(Error, "Failed to parse int in HypClass attribute");
            }

            results.PushBack(Pair<String, HypClassAttributeValue> { key, HypClassAttributeValue(int_value) });
            break;
        }
        case HypClassAttributeType::FLOAT: {
            double float_value;

            if (!StringUtil::Parse<double>(hyp_class_attribute_value_string, &float_value)) {
                return HYP_MAKE_ERROR(Error, "Failed to parse float in HypClass attribute");
            }

            results.PushBack(Pair<String, HypClassAttributeValue> { key, HypClassAttributeValue(float_value) });
            break;
        }
        case HypClassAttributeType::BOOLEAN: {
            bool bool_value;

            if (hyp_class_attribute_value_string == "true") {
                bool_value = true;
            } else if (hyp_class_attribute_value_string == "false") {
                bool_value = false;
            } else {
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

template <class MacroEnumType>
static Result<Pair<MacroEnumType, Array<Pair<String, HypClassAttributeValue>>>> ParseHypMacro(const HashMap<String, MacroEnumType> &usable_macros, const String &line, SizeType &out_start_index, SizeType &out_end_index, bool require_parentheses = true)
{
    out_start_index = String::not_found;
    out_end_index = String::not_found;

    for (const Pair<String, MacroEnumType> &it : usable_macros) {
        SizeType macro_start_index = line.FindFirstIndex(it.first);

        if (macro_start_index != String::not_found) {
            Array<Pair<String, HypClassAttributeValue>> attributes;

            out_start_index = macro_start_index;
            out_end_index = out_start_index + it.first.Length();

            const SizeType parenthesis_index = line.Substr(out_end_index).FindFirstIndex("(");

            if (parenthesis_index == String::not_found) {
                if (require_parentheses) {
                    // Must have parenthesis to be considered an invocation
                    continue;
                }

                // Otherwise, empty attributes are used
            } else {
                out_end_index = out_end_index + parenthesis_index + 1;

                int parenthesis_depth = 1;
                String attributes_string;

                for (; out_end_index < line.Size(); out_end_index++) {
                    if (line[out_end_index] == '(') {
                        parenthesis_depth++;
                    } else if (line[out_end_index] == ')') {
                        parenthesis_depth--;

                        if (parenthesis_depth <= 0) {
                            out_end_index++; // Include the closing parenthesis
                            break;
                        }
                    } else {
                        attributes_string.Append(line[out_end_index]);
                    }
                }

                auto build_attributes_result = BuildHypClassAttributes(attributes_string);

                if (build_attributes_result.HasError()) {
                    return build_attributes_result.GetError();
                }

                attributes = build_attributes_result.GetValue();
            }

            return Pair<MacroEnumType, Array<Pair<String, HypClassAttributeValue>>> { it.second, attributes };
        }
    }

    return Pair<MacroEnumType, Array<Pair<String, HypClassAttributeValue>>> { MacroEnumType::NONE, { } };
}

static Result<Array<HypClassDefinition>, AnalyzerError> BuildHypClasses(const Analyzer &analyzer, Module &mod)
{
    BufferedReader reader;

    if (!mod.GetPath().Open(reader)) {
        return HYP_MAKE_ERROR(AnalyzerError, "Failed to open module", mod.GetPath());
    }

    Array<HypClassDefinition> results;

    Array<String> lines = reader.ReadAllLines();

    for (SizeType i = 0; i < lines.Size(); i++) {
        HypClassDefinition result;

        SizeType macro_start_index;
        SizeType macro_end_index;

        auto parse_macro_result = ParseHypMacro(g_hyp_class_definition_types, lines[i], macro_start_index, macro_end_index, true);

        if (parse_macro_result.HasError()) {
            return AnalyzerError(parse_macro_result.GetError(), mod.GetPath());
        }

        if (parse_macro_result.GetValue().first == HypClassDefinitionType::NONE) {
            continue;
        }

        result.type = parse_macro_result.GetValue().first;
        result.attributes = parse_macro_result.GetValue().second;

        const String content_to_end = String::Join(lines.Slice(i, lines.Size()), '\n');

        const SizeType brace_index = content_to_end.FindFirstIndex("{");

        result.source = content_to_end.Substr(0, brace_index);

        Optional<String> class_name_opt = ExtractCXXClassName(result.source);
        if (!class_name_opt.HasValue()) {
            HYP_LOG(BuildTool, Error, "Failed to extract class name from source: {}", result.source);

            return HYP_MAKE_ERROR(AnalyzerError, "Failed to extract class name", mod.GetPath());
        }

        result.name = *class_name_opt;

        Array<String> base_class_names = ExtractCXXBaseClasses(result.source);
        for (const String &base_class_name : base_class_names) {
            result.base_class_names.PushBack(base_class_name);
        }

        if (brace_index != String::not_found) {
            const String remaining_content = content_to_end.Substr(brace_index + 1, content_to_end.Size());

            int is_in_comment = 0; // 0 = no comment, 1 = line comment, 2 = block comment
            bool is_in_string = false;
            bool is_escaped = false;
            int brace_depth = 1;

            for (SizeType j = 0; j < remaining_content.Size(); j++) {
                const char ch = remaining_content[j];

                result.source.Append(ch);

                if (is_escaped) {
                    is_escaped = false;

                    continue;
                }

                if (ch == '\\') {
                    is_escaped = true;
                } else if (ch == '\n' && is_in_comment == 1) {
                    is_in_comment = 0;
                } else if (ch == '"' && !is_in_comment) {
                    is_in_string = !is_in_string;
                } else if (ch == '/' && !is_in_string && !is_in_comment && j + 1 < remaining_content.Size()) {
                    if (remaining_content[j + 1] == '/') {
                        is_in_comment = 1;
                    } else if (remaining_content[j + 1] == '*') {
                        is_in_comment = 2;
                        j++;
                    }
                } else if (ch == '*' && !is_in_string && is_in_comment == 2 && j + 1 < remaining_content.Size()) {
                    if (remaining_content[j + 1] == '/') {
                        is_in_comment = 0;
                        j++;
                    }
                } else if (!is_in_string && !is_in_comment) {
                    if (ch == '{') {
                        brace_depth++;
                    } else if (ch == '}') {
                        brace_depth--;

                        if (brace_depth <= 0) {
                            result.source.Append(';');

                            break;
                        }
                    }
                }
            }
        }

        HYP_LOG(BuildTool, Info, "Added class \"{}\" with {} attributes and base classes: {}", result.name, result.attributes.Size(), String::Join(result.base_class_names, ','));

        results.PushBack(std::move(result));
    }

    return results;
}

static Result<Array<HypMemberDefinition>, AnalyzerError> BuildHypClassMembers(const Analyzer &analyzer, const Module &mod, const HypClassDefinition &hyp_class_definition)
{
    Array<HypMemberDefinition> results;

    Array<String> lines = hyp_class_definition.source.Split('\n');

    for (SizeType i = 0; i < lines.Size(); i++) {
        const String &line = lines[i];

        SizeType macro_start_index;
        SizeType macro_end_index;

        auto parse_macro_result = ParseHypMacro(g_hyp_member_definition_types, line, macro_start_index, macro_end_index, false);

        if (parse_macro_result.HasError()) {
            return AnalyzerError(parse_macro_result.GetError(), mod.GetPath());
        }

        if (parse_macro_result.GetValue().first == HypMemberType::NONE) {
            continue;
        }

        HypMemberDefinition &result = results.EmplaceBack();
        result.type = parse_macro_result.GetValue().first;
        result.attributes = parse_macro_result.GetValue().second;

        if (result.type == HypMemberType::TYPE_PROPERTY) {
            if (result.attributes.Empty() || result.attributes[0].first.Empty()) {
                return HYP_MAKE_ERROR(AnalyzerError, "Property must have a name", mod.GetPath());
            }

            result.name = result.attributes.PopFront().first;

            continue;
        }

        const String content_to_end = String(line.Substr(macro_end_index)) + "\n" + String::Join(lines.Slice(i + 1, lines.Size()), '\n');

        int is_in_comment = 0; // 0 = no comment, 1 = line comment, 2 = block comment
        bool is_in_string = false;
        bool is_escaped = false;
        int brace_depth = 0;
        int parenthesis_depth = 0;

        for (SizeType j = 0; j < content_to_end.Size(); j++) {
            const char ch = content_to_end[j];

            result.source.Append(ch);

            if (is_escaped) {
                is_escaped = false;

                continue;
            }

            if (ch == '\\') {
                is_escaped = true;
            } else if (ch == '\n' && is_in_comment == 1) {
                is_in_comment = 0;
            } else if (ch == '"' && !is_in_comment) {
                is_in_string = !is_in_string;
            } else if (ch == '/' && !is_in_string && !is_in_comment && j + 1 < content_to_end.Size()) {
                if (content_to_end[j + 1] == '/') {
                    is_in_comment = 1;
                } else if (content_to_end[j + 1] == '*') {
                    is_in_comment = 2;
                    j++;
                }
            } else if (ch == '*' && !is_in_string && is_in_comment == 2 && j + 1 < content_to_end.Size()) {
                if (content_to_end[j + 1] == '/') {
                    is_in_comment = 0;
                    j++;
                }
            } else if (!is_in_string && !is_in_comment) {
                if (ch == '{') {
                    brace_depth++;
                } else if (ch == '}') {
                    brace_depth--;

                    if (brace_depth <= 0 && parenthesis_depth <= 0) {
                        break;
                    }
                } else if (ch == '(') {
                    parenthesis_depth++;
                } else if (ch == ')') {
                    parenthesis_depth--;
                } else if (ch == ';' && brace_depth <= 0) {
                    break;
                }
            }
        }

        SourceFile source_file(mod.GetPath().Basename(), result.source.Size());

        ByteBuffer temp(result.source.Size(), result.source.Data());
        source_file.ReadIntoBuffer(temp);

        TokenStream token_stream(TokenStreamInfo { mod.GetPath().Basename() });

        CompilationUnit unit;
        unit.SetPreprocessorDefinitions(analyzer.GetGlobalDefines());

        const auto CheckParseErrors = [&]() -> Result<void, AnalyzerError>
        {
            String error_message;

            for (SizeType index = 0; index < unit.GetErrorList().Size(); index++) {
                error_message += String::ToString(unit.GetErrorList()[index].GetLocation().GetLine() + 1)
                    + "," + String::ToString(unit.GetErrorList()[index].GetLocation().GetColumn() + 1)
                    + ": " + unit.GetErrorList()[index].GetText() + "\n";
            }

            if (unit.GetErrorList().HasFatalErrors()) {
                return HYP_MAKE_ERROR(AnalyzerError, "Failed to parse member", mod.GetPath(), 0, error_message);
            }

            return { };
        };

        Lexer lexer(SourceStream(&source_file), &token_stream, &unit);
        lexer.Analyze();

        Parser parser(&token_stream, &unit);
        RC<ASTMemberDecl> decl = parser.ParseMemberDecl();

        if (auto res = CheckParseErrors(); res.HasError()) {
            return res.GetError();
        }

        AssertThrow(decl != nullptr);

        result.name = decl->name;
        result.cxx_type = decl->type;
    }

    return results;
}

Result<void, AnalyzerError> ClangDriver::ProcessModule(const Analyzer &analyzer, Module &mod)
{
    auto res = BuildHypClasses(analyzer, mod);

    if (res.HasError()) {
        HYP_LOG(BuildTool, Error, "Failed to build class contents: {}\tError code: {}", res.GetError().GetMessage(), res.GetError().GetErrorCode());

        return res.GetError();
    }

    for (HypClassDefinition &hyp_class_definition : res.GetValue()) {
        if (auto res = BuildHypClassMembers(analyzer, mod, hyp_class_definition); res.GetError()) {
            HYP_LOG(BuildTool, Error, "Failed to build class definition: {}\tError code: {}\tMessage: {}", res.GetError().GetMessage(), res.GetError().GetErrorCode(), res.GetError().GetErrorMessage());

            return res.GetError();
        } else {
            hyp_class_definition.members = std::move(res.GetValue());
        }

        mod.AddHypClassDefinition(std::move(hyp_class_definition));
    }

    return { };
}

} // namespace buildtool
} // namespace hyperion