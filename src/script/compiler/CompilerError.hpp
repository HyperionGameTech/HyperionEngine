#ifndef COMPILER_ERROR_HPP
#define COMPILER_ERROR_HPP

#include <script/SourceLocation.hpp>
#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>

#include <string>
#include <sstream>
#include <map>

namespace hyperion::compiler {

enum ErrorLevel
{
    LEVEL_INFO,
    LEVEL_WARN,
    LEVEL_ERROR
};

enum ErrorMessage
{
    /* Fatal errors */
    Msg_internal_error,
    Msg_custom_error,
    Msg_not_implemented,
    Msg_illegal_syntax,
    Msg_illegal_expression,
    Msg_illegal_operator,
    Msg_invalid_operator_for_type,
    Msg_cannot_overload_operator,
    Msg_invalid_symbol_query,
    Msg_expression_cannot_be_modified,
    Msg_const_missing_assignment,
    Msg_ref_missing_assignment,
    Msg_cannot_create_reference,
    Msg_const_assigned_to_non_const_ref,
    Msg_cannot_modify_rvalue,
    Msg_prohibited_action_attribute,
    Msg_unbalanced_expression,
    Msg_unmatched_parentheses,
    Msg_unexpected_character,
    Msg_unexpected_identifier,
    Msg_unexpected_token,
    Msg_unexpected_eof,
    Msg_unexpected_eol,
    Msg_unrecognized_escape_sequence,
    Msg_unterminated_string_literal,
    Msg_argument_after_varargs,
    Msg_incorrect_number_of_arguments,
    Msg_maximum_number_of_arguments,
    Msg_arg_type_incompatible,
    Msg_incompatible_cast,
    Msg_named_arg_not_found,
    Msg_redeclared_identifier,
    Msg_redeclared_identifier_type,
    Msg_undeclared_identifier,
    Msg_expected_identifier,
    Msg_keyword_cannot_be_used_as_identifier,
    Msg_ambiguous_identifier,
    Msg_invalid_constructor,
    Msg_return_invalid_in_constructor,
    Msg_return_type_specification_invalid_on_constructor,
    Msg_expected_type_got_identifier,
    Msg_missing_type_and_assignment,
    Msg_type_no_default_assignment,
    Msg_could_not_deduce_type_for_expression,
    Msg_expression_not_generic,
    Msg_too_many_generic_args,
    Msg_too_few_generic_args,
    Msg_no_substitution_for_generic_arg,
    Msg_enum_assignment_not_constant,
    Msg_generic_arg_may_not_have_side_effects,

    /* LOOPS */
    Msg_break_outside_loop,
    Msg_continue_outside_loop,

    /* FUNCTIONS */
    Msg_multiple_return_types,
    Msg_mismatched_return_type,
    Msg_must_be_explicitly_marked_any,
    Msg_any_reserved_for_parameters,
    Msg_return_outside_function,
    Msg_yield_outside_function,
    Msg_yield_outside_generator_function,
    Msg_not_a_function,
    Msg_member_not_a_method,
    Msg_closure_capture_must_be_parameter,
    Msg_pure_function_scope,

    /* ARRAYS */
    Msg_invalid_subscript,

    /* TYPES */
    Msg_not_a_type,
    Msg_undefined_type,
    Msg_redefined_type,
    Msg_redefined_builtin_type,
    Msg_type_not_defined_globally,
    Msg_identifier_is_type,
    Msg_cannot_determine_implicit_type,
    Msg_mismatched_types,
    Msg_mismatched_types_assignment,
    Msg_implicit_any_mismatch,
    Msg_type_not_generic,
    Msg_generic_parameters_missing,
    Msg_generic_parameter_redeclared,
    Msg_generic_expression_no_arguments_provided,
    Msg_generic_expression_must_be_const,
    Msg_generic_expression_invalid_arguments,
    Msg_generic_expression_requires_assignment,
    Msg_generic_argument_must_be_literal,
    Msg_not_a_data_member,
    Msg_not_a_constant_type,
    Msg_type_missing_prototype,
    Msg_cannot_inline_variable,

    Msg_bitwise_operands_must_be_int,
    Msg_bitwise_operand_must_be_int,
    Msg_arithmetic_operands_must_be_numbers,
    Msg_arithmetic_operand_must_be_numbers,
    Msg_expected_token,
    Msg_unknown_directive,
    Msg_unknown_module,
    Msg_expected_module,
    Msg_empty_module,
    Msg_module_already_defined,
    Msg_module_not_imported,
    Msg_invalid_module_access,
    Msg_statement_outside_module,
    Msg_module_declared_in_block,
    Msg_could_not_open_file,
    Msg_could_not_find_module,
    Msg_could_not_find_nested_module,
    Msg_identifier_is_module,
    Msg_import_outside_global,
    Msg_import_current_file,
    Msg_export_outside_global,
    Msg_export_invalid_name,
    Msg_export_duplicate,
    Msg_self_outside_class,
    Msg_else_outside_if,
    Msg_proxy_class_cannot_be_constructed,
    Msg_proxy_class_may_only_contain_methods,
    Msg_alias_missing_assignment,
    Msg_alias_must_be_identifier,
    Msg_unrecognized_alias_type,
    Msg_type_contract_outside_definition,
    Msg_unknown_type_contract_requirement,
    Msg_invalid_type_contract_operator,
    Msg_unsatisfied_type_contract,
    Msg_unsupported_feature,

    Msg_unreachable_code,
    Msg_expected_end_of_statement,

    /* Info */
    Msg_unused_identifier,
    Msg_empty_function_body,
    Msg_empty_statement_body,
    Msg_module_name_begins_lowercase,
};

class CompilerError
{
    static const HashMap<ErrorMessage, String> error_message_strings;

public:
    template <typename...Args>
    CompilerError(
        ErrorLevel level, ErrorMessage msg,
        const SourceLocation &location,
        const Args &...args
    ) : m_level(level),
        m_msg(msg),
        m_location(location)
    {
        String msg_str = error_message_strings.At(m_msg);
        MakeMessage(msg_str.Data(), args...);
    }
    
    CompilerError(const CompilerError &other);
    ~CompilerError() = default;

    ErrorLevel GetLevel() const { return m_level; }
    ErrorMessage GetMessage() const { return m_msg; }
    const SourceLocation &GetLocation() const { return m_location; }
    const String &GetText() const { return m_text; }

    bool operator==(const CompilerError &other) const
        { return m_level == other.m_level
            && m_msg == other.m_msg
            && m_location == other.m_location
            && m_text == other.m_text; }

    bool operator!=(const CompilerError &other) const
        { return !(*this == other); }

    bool operator<(const CompilerError &other) const;

private:
    void MakeMessage(const char *format) { m_text += format; }

    template <typename T, typename ... Args>
    void MakeMessage(const char *format, T value, Args && ... args)
    {
        for (; *format; format++) {
            if (*format == '%') {
                std::stringstream sstream;
                sstream << value;
                m_text += sstream.str().c_str();
                MakeMessage(format + 1, args...);
                return;
            }

            m_text += *format;
        }
    }

    ErrorLevel      m_level;
    ErrorMessage    m_msg;
    SourceLocation  m_location;
    String          m_text;
};

} // namespace hyperion::compiler

#endif
