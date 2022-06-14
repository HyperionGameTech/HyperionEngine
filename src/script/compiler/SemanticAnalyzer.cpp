#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/debug.h>

#include <set>
#include <iostream>
#include <sstream>

namespace hyperion::compiler {

SemanticAnalyzer::Helpers::IdentifierLookupResult
SemanticAnalyzer::Helpers::LookupIdentifier(
    AstVisitor *visitor,
    Module *mod,
    const std::string &name)
{
    IdentifierLookupResult res;
    res.type = IDENTIFIER_TYPE_UNKNOWN;

    // the variable must exist in the active scope or a parent scope
    if ((res.as_identifier = mod->LookUpIdentifier(name, false))) {
        res.type = IDENTIFIER_TYPE_VARIABLE;
    } else if ((res.as_identifier = visitor->GetCompilationUnit()->GetGlobalModule()->LookUpIdentifier(name, false))) {
        // if the identifier was not found,
        // look in the global module to see if it is a global function.
        res.type = IDENTIFIER_TYPE_VARIABLE;
    } else if ((res.as_module = visitor->GetCompilationUnit()->LookupModule(name))) {
        res.type = IDENTIFIER_TYPE_MODULE;
    } else if ((res.as_type = mod->LookupSymbolType(name))) {
        res.type = IDENTIFIER_TYPE_TYPE;
    } else {
        // nothing was found
        res.type = IDENTIFIER_TYPE_NOT_FOUND;
    }
    
    return res;
}

void CheckArgTypeCompatible(
    AstVisitor *visitor,
    const SourceLocation &location,
    const SymbolTypePtr_t &arg_type,
    const SymbolTypePtr_t &param_type)
{
    AssertThrow(arg_type != nullptr);
    AssertThrow(param_type != nullptr);

    // make sure argument types are compatible
    // use strict numbers so that floats cannot be passed as explicit ints
    // @NOTE: do not add error for undefined, it causes too many unnecessary errors
    //        that would've already been conveyed via 'not declared' errors
    if (arg_type != BuiltinTypes::UNDEFINED) {
        if (!param_type->TypeCompatible(*arg_type, false)) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_arg_type_incompatible,
                location,
                arg_type->GetName(),
                param_type->GetName()
            ));
        }
    }
}

struct ArgInfo {
    bool is_named;
    std::string name;
    SymbolTypePtr_t type;
};

static int FindFreeSlot(
    int current_index,
    const std::set<int> &used_indices,
    const std::vector<GenericInstanceTypeInfo::Arg> &generic_args,
    bool is_variadic = false,
    int num_supplied_args = -1)
{
    const size_t num_params = generic_args.size();
    
    for (size_t counter = 0; counter < num_params; counter++) {
        // variadic keeps counting
        if (!is_variadic && current_index == num_params) {
            current_index = 0;
        }

        // check if index is used
        if (used_indices.find(current_index) == used_indices.end()) {
            // not found, return the index
            return current_index;
        }

        current_index++;
    }

    // no slot available
    return -1;
}

static int ArgIndex(
    int current_index,
    const ArgInfo &arg_info,
    const std::set<int> &used_indices,
    const std::vector<GenericInstanceTypeInfo::Arg> &generic_args,
    bool is_variadic = false,
    int num_supplied_args = -1)
{
    if (arg_info.is_named) {
        for (int j = 0; j < generic_args.size(); j++) {
            const std::string &generic_arg_name = generic_args[j].m_name;
            if (generic_arg_name == arg_info.name && used_indices.find(j) == used_indices.end()) {
                return j;
            }
        }

        return -1;
    }

    return FindFreeSlot(
        current_index,
        used_indices,
        generic_args,
        is_variadic,
        num_supplied_args
    );
}

std::vector<std::shared_ptr<AstArgument>> SemanticAnalyzer::Helpers::SubstituteGenericArgs(
    AstVisitor *visitor, Module *mod,
    const std::vector<GenericInstanceTypeInfo::Arg> &generic_args,
    const std::vector<std::shared_ptr<AstArgument>> &args,
    const SourceLocation &location)
{
    std::vector<std::shared_ptr<AstArgument>> res_args;
    res_args.resize(args.size());

    // check for varargs (at end)
    bool is_varargs = false;
    SymbolTypePtr_t vararg_type;

    if (!generic_args.empty()) {
        const SymbolTypePtr_t &last_generic_arg_type = generic_args.back().m_type;
        AssertThrow(last_generic_arg_type != nullptr);

        if (last_generic_arg_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
            const SymbolTypePtr_t arg_base = last_generic_arg_type->GetBaseType();
            AssertThrow(arg_base != nullptr);

            // check if it is an instance of varargs type
            if (arg_base == BuiltinTypes::VAR_ARGS) {
                is_varargs = true;

                AssertThrow(!last_generic_arg_type->GetGenericInstanceInfo().m_generic_args.empty());
                vararg_type = last_generic_arg_type->GetGenericInstanceInfo().m_generic_args.front().m_type;
            }
        }
    }

    std::set<int> used_indices;

    if (generic_args.size() <= args.size() || (is_varargs && generic_args.size() - 1 <= args.size())) {
        using ArgDataPair = std::pair<ArgInfo, std::shared_ptr<AstArgument>>;

        std::vector<ArgDataPair> named_args;
        std::vector<ArgDataPair> unnamed_args;

        // sort into two separate buckets
        for (size_t i = 0; i < args.size(); i++) {
            AssertThrow(args[i] != nullptr);

            ArgInfo arg_info;
            arg_info.is_named = args[i]->IsNamed();
            arg_info.name = args[i]->GetName();
            arg_info.type = args[i]->GetExprType();

            ArgDataPair arg_data_pair = {
                arg_info,
                args[i]
            };

            if (arg_info.is_named) {
                named_args.push_back(arg_data_pair);
            } else {
                unnamed_args.push_back(arg_data_pair);
            }
        }

        // handle named arguments first
        for (size_t i = 0; i < named_args.size(); i++) {
            const ArgDataPair &arg = named_args[i];

            const int found_index = ArgIndex(
                i,
                std::get<0>(arg),
                used_indices,
                generic_args
            );

            if (found_index != -1) {
                used_indices.insert(found_index);

                // found successfully, check type compatibility
                const SymbolTypePtr_t &param_type = generic_args[found_index].m_type;

                CheckArgTypeCompatible(
                    visitor,
                    std::get<1>(arg)->GetLocation(),
                    std::get<0>(arg).type,
                    param_type
                );

                res_args[found_index] = std::get<1>(arg);
            } else {
                std::stringstream ss;

                ss << "[";

                for (size_t i = 0; i < generic_args.size(); i++) {
                    const auto &arg = generic_args[i];

                    if (!arg.m_name.empty()) {
                        ss << arg.m_name << ": " << (arg.m_type != nullptr ? arg.m_type->GetName() : "??");
                    } else {
                        ss << "$" << i << ": " << (arg.m_type != nullptr ? arg.m_type->GetName() : "??");
                    }

                    if (i != generic_args.size() - 1) {
                        ss << ", ";
                    }
                }

                ss << "]";

                // not found so add error
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_named_arg_not_found,
                    std::get<1>(arg)->GetLocation(),
                    std::get<0>(arg).name,
                    ss.str()
                ));
            }
        }

        // handle unnamed arguments
        for (size_t i = 0; i < unnamed_args.size(); i++) {
            const ArgDataPair &arg = unnamed_args[i];

            const int found_index = ArgIndex(
                i,
                std::get<0>(arg),
                used_indices,
                generic_args,
                is_varargs
            );

            if (is_varargs && ((i + named_args.size())) >= generic_args.size() - 1) {
                // in varargs... check against vararg base type
                CheckArgTypeCompatible(
                    visitor,
                    std::get<1>(arg)->GetLocation(),
                    std::get<0>(arg).type,
                    vararg_type
                );

                if (found_index == -1 || found_index >= res_args.size()) {
                    used_indices.insert(res_args.size());
                    // at end, push
                    res_args.push_back(std::get<1>(arg));
                } else {
                    res_args[found_index] = std::get<1>(arg);
                    used_indices.insert(found_index);
                }
            } else {
                if (found_index == -1) {
                    // too many args supplied
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_incorrect_number_of_arguments,
                        location,
                        generic_args.size(),
                        args.size()
                    ));
                } else {
                    // store used index
                    used_indices.insert(found_index);

                    // choose whether to get next param, or last (in the case of varargs)
                    struct {
                        std::string name;
                        SymbolTypePtr_t type;    
                    } param_info;

                    const GenericInstanceTypeInfo::Arg &param = (found_index < generic_args.size())
                        ? generic_args[found_index]
                        : generic_args.back();
                    
                    param_info.name = param.m_name;
                    param_info.type = param.m_type;

                    CheckArgTypeCompatible(
                        visitor,
                        std::get<1>(arg)->GetLocation(),
                        std::get<0>(arg).type,
                        param_info.type
                    );

                    res_args[found_index] = std::get<1>(arg);
                }
            }
        }
    } else {
        // wrong number of args given
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_incorrect_number_of_arguments,
            location,
            is_varargs ? generic_args.size() - 1 : generic_args.size(),
            args.size()
        ));
    }

    // return the "return type" of the function
    return res_args;
}

FunctionTypeSignature_t SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
    AstVisitor *visitor, Module *mod, 
    const SymbolTypePtr_t &identifier_type, 
    const std::vector<std::shared_ptr<AstArgument>> &args,
    const SourceLocation &location)
{
    // BuiltinTypes::FUNCTION, BuiltinTypes::GENERIC_VARIABLE_TYPE, etc.
    if (identifier_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
        const SymbolTypePtr_t base = identifier_type->GetBaseType();

        const auto &generic_args = identifier_type->GetGenericInstanceInfo().m_generic_args;

        // make sure the "return type" of the function is not null
        AssertThrow(generic_args.size() >= 1);
        AssertThrow(generic_args[0].m_type != nullptr);

        const std::vector<GenericInstanceTypeInfo::Arg> generic_args_without_return(generic_args.begin() + 1, generic_args.end());
        const auto res_args = SemanticAnalyzer::Helpers::SubstituteGenericArgs(visitor, mod, generic_args_without_return, args, location);

        // return the "return type" of the function
        return FunctionTypeSignature_t {
            generic_args[0].m_type, res_args
        };
    }
    
    return FunctionTypeSignature_t {
        BuiltinTypes::ANY, args
    };
}

void SemanticAnalyzer::Helpers::EnsureLooseTypeAssignmentCompatibility(
    AstVisitor *visitor,
    Module *mod,
    const SymbolTypePtr_t &symbol_type,
    const SymbolTypePtr_t &assignment_type,
    const SourceLocation &location)
{
    AssertThrow(symbol_type != nullptr);
    AssertThrow(assignment_type != nullptr);

    // symbol_type should be the user-specified type
    SymbolTypePtr_t symbol_type_promoted = SymbolType::GenericPromotion(symbol_type, assignment_type);
    AssertThrow(symbol_type_promoted != nullptr);

    // generic not yet promoted to an instance
    if (symbol_type_promoted->GetTypeClass() == TYPE_GENERIC) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_generic_parameters_missing,
            location,
            symbol_type_promoted->GetName(),
            symbol_type_promoted->GetGenericInfo().m_num_parameters
        ));
    }

    SymbolTypePtr_t comparison_type = symbol_type;

    // unboxing values
    // note that this is below the default assignment check,
    // because these "box" types may have a default assignment of their own
    // (or they may intentionally not have one)
    // e.g Maybe(T) defaults to null, and Const(T) has no assignment.
    if (symbol_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
        if (symbol_type->IsBoxedType()) {
            comparison_type = symbol_type->GetGenericInstanceInfo().m_generic_args[0].m_type;
            AssertThrow(comparison_type != nullptr);
        }
    }

    SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
        visitor,
        mod,
        comparison_type,
        assignment_type,
        location
    );
}

void SemanticAnalyzer::Helpers::EnsureTypeAssignmentCompatibility(
    AstVisitor *visitor,
    Module *mod,
    const SymbolTypePtr_t &symbol_type,
    const SymbolTypePtr_t &assignment_type,
    const SourceLocation &location)
{
    AssertThrow(symbol_type != nullptr);
    AssertThrow(assignment_type != nullptr);

    if (!symbol_type->TypeCompatible(*assignment_type, false)) {
        CompilerError error(
            LEVEL_ERROR,
            Msg_mismatched_types_assignment,
            location,
            symbol_type->GetName(),
            assignment_type->GetName()
        );

        if (assignment_type == BuiltinTypes::ANY) {
            error = CompilerError(
                LEVEL_ERROR,
                Msg_implicit_any_mismatch,
                location,
                symbol_type->GetName()
            );
        }

        visitor->GetCompilationUnit()->GetErrorList().AddError(error);
    }
}

SemanticAnalyzer::SemanticAnalyzer(
    AstIterator *ast_iterator,
    CompilationUnit *compilation_unit)
    : AstVisitor(ast_iterator, compilation_unit)
{
}

SemanticAnalyzer::SemanticAnalyzer(const SemanticAnalyzer &other)
    : AstVisitor(other.m_ast_iterator, other.m_compilation_unit)
{
}

void SemanticAnalyzer::Analyze(bool expect_module_decl)
{
    Module *mod = m_compilation_unit->GetCurrentModule();
    AssertThrow(mod != nullptr);

    while (m_ast_iterator->HasNext()) {
        auto node = m_ast_iterator->Next();
        AssertThrow(node != nullptr);

        node->Visit(this, mod);
    }
}

} // namespace hyperion::compiler
