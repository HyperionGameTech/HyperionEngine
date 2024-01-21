#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstUndefined.hpp>
#include <script/compiler/Module.hpp>

#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/Debug.hpp>

#include <set>
#include <iostream>
#include <sstream>

#include "ast/AstTypeObject.hpp"

namespace hyperion::compiler {

void SemanticAnalyzer::Helpers::CheckArgTypeCompatible(
    AstVisitor *visitor,
    const SourceLocation &location,
    const SymbolTypePtr_t &arg_type,
    const SymbolTypePtr_t &param_type
)
{
    AssertThrow(arg_type != nullptr);
    AssertThrow(param_type != nullptr);

    // make sure argument types are compatible
    // use strict numbers so that floats cannot be passed as explicit ints
    // @NOTE: do not add error for undefined, it causes too many unnecessary errors
    //        that would've already been conveyed via 'not declared' errors
    if (arg_type != BuiltinTypes::UNDEFINED) {
        if (!param_type->TypeCompatible(*arg_type, true)) {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_arg_type_incompatible,
                location,
                arg_type->ToString(),
                param_type->ToString()
            ));
        }
    }
}

Int SemanticAnalyzer::Helpers::FindFreeSlot(
    int current_index,
    const std::set<int> &used_indices,
    const Array<GenericInstanceTypeInfo::Arg> &generic_args,
    bool is_variadic,
    int num_supplied_args
)
{
    const SizeType num_params = generic_args.Size();
    
    for (SizeType counter = 0; counter < num_params; counter++) {
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

Int SemanticAnalyzer::Helpers::ArgIndex(
    Int current_index,
    const ArgInfo &arg_info,
    const std::set<Int> &used_indices,
    const Array<GenericInstanceTypeInfo::Arg> &generic_args,
    Bool is_variadic,
    Int num_supplied_args
)
{
    if (arg_info.is_named) {
        for (int j = 0; j < generic_args.Size(); j++) {
            const String &generic_arg_name = generic_args[j].m_name;

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

SymbolTypePtr_t SemanticAnalyzer::Helpers::SubstituteGenericParameters(
    AstVisitor *visitor,
    Module *mod,
    const SymbolTypePtr_t &input_type,
    const Array<GenericInstanceTypeInfo::Arg> &generic_args,
    const Array<SubstitutionResult> &substitution_results,
    const SourceLocation &location
)
{
    if (!input_type) {
        return nullptr;
    }

    const auto FindArgForInputType = [&]() -> const SubstitutionResult *
    {
        Int generic_arg_index = -1;

        for (SizeType index = 0; index < generic_args.Size(); index++) {
            if (generic_args[index].m_name == input_type->GetName()) {
                generic_arg_index = Int(index);

                break;
            }
        }

        const auto substitution_result_it = substitution_results.FindIf([generic_arg_index](const SubstitutionResult &substitution_result) {
            return substitution_result.index == generic_arg_index;
        });

        if (substitution_result_it == substitution_results.End()) {
            return nullptr;
        }
        
        return substitution_result_it;
    };

    switch (input_type->GetTypeClass()) {
    case TYPE_GENERIC_PARAMETER: {
        const SubstitutionResult *found_substitution_result = FindArgForInputType();

        if (found_substitution_result) {
            AssertThrow(found_substitution_result->arg != nullptr);

            const GenericInstanceTypeInfo::Arg &generic_arg = generic_args[found_substitution_result->index];

            // @TODO We need to reevaluate the order of which Arguments are visited VS. this chain of methods gets called.
            /// We need a way to mark ref/const BEFORE visiting OR we need to ignore it in the first visiting stage and just use it for building..
            /// // Or we could do first visit, mark ref/const, then clone + visit again.

            found_substitution_result->arg->SetIsPassByRef(generic_arg.m_is_ref);
            found_substitution_result->arg->SetIsPassConst(generic_arg.m_is_const);

            auto *deep_value_of = found_substitution_result->arg->GetDeepValueOf();
            AssertThrow(deep_value_of != nullptr);

            if (SymbolTypePtr_t held_type = deep_value_of->GetHeldType()) {
                if (!held_type->IsOrHasBase(*BuiltinTypes::UNDEFINED)) {
                    return held_type;
                }
            }
        }

        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_no_substitution_for_generic_arg,
            location,
            input_type->GetName()
        ));

        return BuiltinTypes::UNDEFINED;
    }
    case TYPE_GENERIC_INSTANCE: {
        auto base_type = input_type->GetBaseType();
        AssertThrow(base_type != nullptr);

        /*base_type = SubstituteGenericParameters(
            visitor,
            mod,
            base_type,
            args,
            location
        );*/

        const auto &generic_instance_info = input_type->GetGenericInstanceInfo();

        Array<GenericInstanceTypeInfo::Arg> res_args = generic_instance_info.m_generic_args;

        for (SizeType index = 0; index < generic_instance_info.m_generic_args.Size(); index++) {
            res_args[index].m_type = SubstituteGenericParameters(
                visitor,
                mod,
                generic_instance_info.m_generic_args[index].m_type,
                generic_args,
                substitution_results,
                location
            );
        }

        SymbolTypePtr_t substituted_type = SymbolType::GenericInstance(
            base_type,
            GenericInstanceTypeInfo {
                std::move(res_args)
            }
        );

        for (const SymbolMember_t &member : input_type->GetMembers()) {
            substituted_type->AddMember({
                std::get<0>(member),
                SubstituteGenericParameters(
                    visitor,
                    mod,
                    std::get<1>(member),
                    generic_args,
                    substitution_results,
                    location
                ),
                std::get<2>(member)
            });
        }

        return substituted_type;
    }
    default:
        return input_type;
    }
}

FunctionTypeSignature_t SemanticAnalyzer::Helpers::ExtractGenericArgs(
    AstVisitor *visitor,
    Module *mod,
    const SymbolTypePtr_t &identifier_type, 
    const Array<RC<AstArgument>> &args,
    const SourceLocation &location,
    Array<SubstitutionResult>(*fn) (
        AstVisitor *visitor,
        Module *mod,
        const Array<GenericInstanceTypeInfo::Arg> &generic_args,
        const Array<RC<AstArgument>> &args,
        const SourceLocation &location
    )
)
{
    // BuiltinTypes::FUNCTION, BuiltinTypes::GENERIC_VARIABLE_TYPE, etc.
    if (identifier_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
        const SymbolTypePtr_t base = identifier_type->GetBaseType();

        const auto &generic_args = identifier_type->GetGenericInstanceInfo().m_generic_args;

        // make sure the "return type" of the function is not null
        AssertThrow(generic_args.Size() >= 1);
        AssertThrow(generic_args[0].m_type != nullptr);

        const Array<GenericInstanceTypeInfo::Arg> generic_args_without_return(generic_args.Begin() + 1, generic_args.End());

        const auto substitution_results = fn(
            visitor,
            mod,
            generic_args_without_return,
            args,
            location
        );

        // Error occurred
        if (substitution_results.Any([](const auto &item) { return item.arg == nullptr; })) {
            return FunctionTypeSignature_t {
               BuiltinTypes::UNDEFINED,
               args
            };
        }
        
        // replace generics used within the "return type" of the function/generic
        const auto return_type = SubstituteGenericParameters(
            visitor,
            mod,
            generic_args[0].m_type,
            generic_args_without_return,
            substitution_results,
            location
        );

        AssertThrow(return_type != nullptr);

        Array<RC<AstArgument>> res_args;
        res_args.Reserve(substitution_results.Size());

        for (const auto &substitution_result : substitution_results) {
            res_args.PushBack(substitution_result.arg);
        }

        return FunctionTypeSignature_t {
            return_type,
            res_args
        };
    }
    
    return FunctionTypeSignature_t {
        BuiltinTypes::ANY,
        args
    };
}

SymbolTypePtr_t SemanticAnalyzer::Helpers::GetVarArgType(const Array<GenericInstanceTypeInfo::Arg> &generic_args)
{
    if (generic_args.Empty()) {
        return nullptr;
    }

    const SymbolTypePtr_t &last_generic_arg_type = generic_args.Back().m_type;
    AssertThrow(last_generic_arg_type != nullptr);

    if (last_generic_arg_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
        const SymbolTypePtr_t arg_base = last_generic_arg_type->GetBaseType();
        AssertThrow(arg_base != nullptr);

        // check if it is an instance of varargs type
        if (arg_base == BuiltinTypes::VAR_ARGS) {
            AssertThrow(!last_generic_arg_type->GetGenericInstanceInfo().m_generic_args.Empty());

            return last_generic_arg_type->GetGenericInstanceInfo().m_generic_args.Front().m_type;
        }
    }

    return nullptr;
}

void SemanticAnalyzer::Helpers::EnsureFunctionArgCompatibility(
    AstVisitor *visitor,
    Module *mod,
    const SymbolTypePtr_t &identifier_type, 
    const Array<RC<AstArgument>> &args,
    const SourceLocation &location
)
{
    ExtractGenericArgs(
        visitor,
        mod,
        identifier_type,
        args,
        location,
        [](
            AstVisitor *visitor, Module *mod,
            const Array<GenericInstanceTypeInfo::Arg> &generic_args,
            const Array<RC<AstArgument>> &args,
            const SourceLocation &location
        )
        {
            SymbolTypePtr_t vararg_type = GetVarArgType(generic_args);

            const SizeType num_generic_args = vararg_type != nullptr
                ? generic_args.Size() - 1
                : generic_args.Size();

            for (SizeType index = 0; index < args.Size(); index++) {
                if (index >= num_generic_args && vararg_type != nullptr) {
                    CheckArgTypeCompatible(
                        visitor,
                        args[index]->GetLocation(),
                        args[index]->GetExprType(),
                        vararg_type
                    );
                } else if (index < num_generic_args && generic_args[index].m_type != nullptr) {
                    CheckArgTypeCompatible(
                        visitor,
                        args[index]->GetLocation(),
                        args[index]->GetExprType(),
                        generic_args[index].m_type
                    );
                } else {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_incorrect_number_of_arguments,
                        location,
                        generic_args.Size(),
                        args.Size()
                    ));
                }
            }

            Array<SubstitutionResult> substitution_results;
            substitution_results.Resize(args.Size());

            for (SizeType index = 0; index < args.Size(); index++) {
                substitution_results[index].arg = args[index];
                substitution_results[index].index = Int(index);
            }

            return substitution_results;
        }
    );
}

FunctionTypeSignature_t SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
    AstVisitor *visitor, Module *mod, 
    const SymbolTypePtr_t &identifier_type, 
    const Array<RC<AstArgument>> &args,
    const SourceLocation &location
)
{
    return ExtractGenericArgs(
        visitor,
        mod,
        identifier_type,
        args,
        location,
        [](
            AstVisitor *visitor, Module *mod,
            const Array<GenericInstanceTypeInfo::Arg> &generic_args,
            const Array<RC<AstArgument>> &args,
            const SourceLocation &location
        )
        {
            SymbolTypePtr_t vararg_type = GetVarArgType(generic_args);

            const SizeType num_generic_args = vararg_type != nullptr
                ? generic_args.Size() - 1
                : generic_args.Size();

            SizeType num_generic_args_without_default_assigned = num_generic_args;

            for (SizeType index = 0; index < num_generic_args; ++index) {
                if (generic_args[index].m_default_value != nullptr) {
                    --num_generic_args_without_default_assigned;
                }
            }

            std::set<int> used_indices;

            Array<SubstitutionResult> substitution_results;
            substitution_results.Resize(num_generic_args);

            if (num_generic_args_without_default_assigned <= args.Size()) {
                using ArgDataPair = std::pair<ArgInfo, RC<AstArgument>>;

                Array<ArgDataPair> named_args;
                Array<ArgDataPair> unnamed_args;

                // sort into two separate buckets
                for (SizeType i = 0; i < args.Size(); i++) {
                    AssertThrow(args[i] != nullptr);

                    ArgInfo arg_info;
                    arg_info.is_named = args[i]->IsNamed();
                    arg_info.name = args[i]->GetName();

                    ArgDataPair arg_data_pair = {
                        arg_info,
                        args[i]
                    };

                    if (arg_info.is_named) {
                        named_args.PushBack(std::move(arg_data_pair));
                    } else {
                        unnamed_args.PushBack(std::move(arg_data_pair));
                    }
                }

                // handle named arguments first
                for (SizeType i = 0; i < named_args.Size(); i++) {
                    const ArgDataPair &arg = named_args[i];
                    AssertThrow(std::get<1>(arg) != nullptr);

                    const int found_index = ArgIndex(
                        i,
                        std::get<0>(arg),
                        used_indices,
                        generic_args
                    );

                    if (found_index != -1) {
                        used_indices.insert(found_index);

                        // found successfully, check type compatibility
                        // const SymbolTypePtr_t &param_type = generic_args[found_index].m_type;

                        // CheckArgTypeCompatible(
                        //     visitor,
                        //     std::get<1>(arg)->GetLocation(),
                        //     std::get<0>(arg).type,
                        //     param_type
                        // );

                        AssertThrow(SizeType(found_index) < substitution_results.Size());
                        
                        RC<AstArgument> arg_type = std::get<1>(arg);
                        AssertThrow(arg_type != nullptr);

                        substitution_results[found_index].arg = arg_type;
                        substitution_results[found_index].arg->SetIsPassByRef(generic_args[found_index].m_is_ref);
                        substitution_results[found_index].arg->SetIsPassConst(generic_args[found_index].m_is_const);
                        substitution_results[found_index].index = found_index;
                    } else {
                        std::stringstream ss;

                        ss << "[";

                        for (SizeType i = 0; i < generic_args.Size(); i++) {
                            const auto &arg = generic_args[i];

                            if (arg.m_name.Any()) {
                                ss << arg.m_name << ": " << (arg.m_type != nullptr ? arg.m_type->ToString() : "??");
                            } else {
                                ss << "$" << i << ": " << (arg.m_type != nullptr ? arg.m_type->ToString() : "??");
                            }

                            if (i != generic_args.Size() - 1) {
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
                for (SizeType i = 0; i < unnamed_args.Size(); i++) {
                    const ArgDataPair &arg = unnamed_args[i];
                    AssertThrow(std::get<1>(arg) != nullptr);

                    Int found_index = ArgIndex(
                        i,
                        std::get<0>(arg),
                        used_indices,
                        generic_args,
                        vararg_type != nullptr
                    );

                    if (vararg_type != nullptr && ((i + named_args.Size())) >= generic_args.Size() - 1) {
                        // in varargs... check against vararg base type
                        // CheckArgTypeCompatible(
                        //     visitor,
                        //     std::get<1>(arg)->GetLocation(),
                        //     std::get<0>(arg).type,
                        //     vararg_type
                        // );

                        // check should not be neccessary but added just in case
                        const bool is_ref = generic_args.Size() != 0
                            ? generic_args[generic_args.Size() - 1].m_is_ref
                            : false;

                        const bool is_const = generic_args.Size() != 0
                            ? generic_args[generic_args.Size() - 1].m_is_const
                            : false;
                        
                        RC<AstArgument> arg_type = std::get<1>(arg);
                        AssertThrow(arg_type != nullptr);

                        arg_type->SetIsPassByRef(is_ref);
                        arg_type->SetIsPassConst(is_const);

                        if (found_index == -1 || found_index >= substitution_results.Size()) {
                            found_index = Int(substitution_results.Size());

                            // at end, push to make room
                            substitution_results.Resize(substitution_results.Size() + 1);
                        }

                        AssertThrow(found_index < substitution_results.Size());

                        used_indices.insert(found_index);

                        substitution_results[found_index] = {
                            std::move(arg_type),
                            found_index
                        };
                    } else if (found_index != -1) {
                        AssertThrow(found_index < substitution_results.Size());

                        // store used index
                        used_indices.insert(found_index);

                        const GenericInstanceTypeInfo::Arg &param = (found_index < generic_args.Size())
                            ? generic_args[found_index]
                            : generic_args.Back();
                        
                        RC<AstArgument> arg_type = std::get<1>(arg);
                        AssertThrow(arg_type != nullptr);

                        arg_type->SetIsPassByRef(param.m_is_ref);
                        arg_type->SetIsPassConst(param.m_is_const);

                        substitution_results[found_index] = {
                            std::move(arg_type),
                            found_index
                        };
                    } else {
                        // too many args supplied
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_incorrect_number_of_arguments,
                            location,
                            generic_args.Size(),
                            args.Size()
                        ));
                    }
                }
            
                // handle arguments that weren't passed in, but have default assignments.
                Array<SizeType> unused_indices;

                for (SizeType index = 0; index < num_generic_args; ++index) {
                    if (!unused_indices.Contains(index) && !used_indices.contains(index)) {
                        unused_indices.PushBack(index);
                    }
                }

                Int unused_index_counter = 0;

                for (auto it = unused_indices.Begin(); it != unused_indices.End();) {
                    const SizeType unused_index = *it;

                    AssertThrow(unused_index < generic_args.Size());
            
                    const bool has_default_value = generic_args[unused_index].m_default_value != nullptr;
                    const bool is_ref = generic_args[unused_index].m_is_ref;
                    const bool is_const = generic_args[unused_index].m_is_const;

                    RC<AstArgument> substituted_arg;

                    {
                        RC<AstExpression> expr;

                        if (has_default_value) {
                            expr = CloneAstNode(generic_args[unused_index].m_default_value);
                        } else {
                            expr.Reset(new AstUndefined(location));
                        }

                        substituted_arg.Reset(new AstArgument(
                            expr,
                            false,
                            true,
                            is_ref,
                            is_const,
                            generic_args[unused_index].m_name,
                            location
                        ));
                    }
                    
                    // push the default value as argument
                    // use named argument, same name as in definition
                    
                    substituted_arg->Visit(visitor, mod);

                    ArgInfo arg_info;
                    arg_info.is_named   = substituted_arg->IsNamed();
                    arg_info.name       = substituted_arg->GetName();
                    arg_info.type       = substituted_arg->GetExprType();

                    Int found_index = ArgIndex(
                        unused_index_counter,
                        arg_info,
                        used_indices,
                        generic_args
                    );

                    if (found_index == -1) {
                        found_index = Int(substitution_results.Size());
                        substitution_results.Resize(substitution_results.Size() + 1);
                    }

                    AssertThrow(SizeType(found_index) < substitution_results.Size());
                    substitution_results[found_index] = {
                        std::move(substituted_arg),
                        found_index
                    };
                
                    if (!has_default_value) {
                        // not provided and no default value
                        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_generic_expression_invalid_arguments,
                            location,
                            generic_args[unused_index].m_name
                        ));
                    }

                    ++unused_index_counter;

                    it = unused_indices.Erase(it);
                }

                if (unused_indices.Any()) {
                    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_incorrect_number_of_arguments,
                        location,
                        num_generic_args_without_default_assigned,
                        args.Size()
                    ));
                }
            } else {
                // wrong number of args given
                visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_incorrect_number_of_arguments,
                    location,
                    num_generic_args_without_default_assigned,
                    args.Size()
                ));
            }

            return substitution_results;
        }
    );
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
            symbol_type_promoted->ToString(),
            symbol_type_promoted->GetGenericInfo().m_num_parameters
        ));
    }

    SymbolTypePtr_t comparison_type = symbol_type;

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
    const SourceLocation &location
)
{
    AssertThrow(symbol_type != nullptr);
    AssertThrow(assignment_type != nullptr);

    if (!symbol_type->TypeCompatible(*assignment_type, true)) {
        CompilerError error(
            LEVEL_ERROR,
            Msg_mismatched_types_assignment,
            location,
            symbol_type->ToString(),
            assignment_type->ToString()
        );

        if (assignment_type->IsAnyType()) {
            error = CompilerError(
                LEVEL_ERROR,
                Msg_implicit_any_mismatch,
                location,
                symbol_type->ToString()
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

        node->SetScopeDepth(m_compilation_unit->GetCurrentModule()->m_scopes.TopNode()->m_depth);

        node->Visit(this, mod);
    }
}

} // namespace hyperion::compiler
