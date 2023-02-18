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

namespace hyperion::compiler {

void CheckArgTypeCompatible(
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

struct ArgInfo
{
    bool is_named;
    std::string name;
    SymbolTypePtr_t type;
};

static int FindFreeSlot(
    int current_index,
    const std::set<int> &used_indices,
    const Array<GenericInstanceTypeInfo::Arg> &generic_args,
    bool is_variadic = false,
    int num_supplied_args = -1)
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

static int ArgIndex(
    int current_index,
    const ArgInfo &arg_info,
    const std::set<int> &used_indices,
    const Array<GenericInstanceTypeInfo::Arg> &generic_args,
    bool is_variadic = false,
    int num_supplied_args = -1)
{
    if (arg_info.is_named) {
        for (int j = 0; j < generic_args.Size(); j++) {
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

static FunctionTypeSignature_t ExtractGenericArgs(
    AstVisitor *visitor,
    Module *mod,
    const SymbolTypePtr_t &identifier_type, 
    const Array<RC<AstArgument>> &args,
    const SourceLocation &location,
    Array<RC<AstArgument>>(*fn) (
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

        const auto res_args = fn(
            visitor,
            mod,
            generic_args_without_return,
            args,
            location
        );

        // return the "return type" of the function
        return FunctionTypeSignature_t {
            generic_args[0].m_type,
            res_args
        };
    }
    
    return FunctionTypeSignature_t {
        BuiltinTypes::ANY,
        args
    };
}

static SymbolTypePtr_t GetVarArgType(
    const Array<GenericInstanceTypeInfo::Arg> &generic_args
)
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

            return args;
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

            Array<RC<AstArgument>> res_args;
            res_args.Resize(num_generic_args);

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
                        named_args.PushBack(arg_data_pair);
                    } else {
                        unnamed_args.PushBack(arg_data_pair);
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

                        AssertThrow(found_index < res_args.Size());

                        res_args[found_index] = std::get<1>(arg);
                        res_args[found_index]->SetIsPassByRef(generic_args[found_index].m_is_ref);
                        res_args[found_index]->SetIsPassConst(generic_args[found_index].m_is_const);
                    } else {
                        std::stringstream ss;

                        ss << "[";

                        for (SizeType i = 0; i < generic_args.Size(); i++) {
                            const auto &arg = generic_args[i];

                            if (!arg.m_name.empty()) {
                                ss << arg.m_name << ": " << (arg.m_type != nullptr ? arg.m_type->GetName() : "??");
                            } else {
                                ss << "$" << i << ": " << (arg.m_type != nullptr ? arg.m_type->GetName() : "??");
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

                    const int found_index = ArgIndex(
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

                        if (found_index == -1 || found_index >= res_args.Size()) {
                            used_indices.insert(res_args.Size());

                            // at end, push
                            res_args.PushBack(std::get<1>(arg));
                            res_args.Back()->SetIsPassByRef(is_ref);
                            res_args.Back()->SetIsPassConst(is_const);
                        } else {
                            used_indices.insert(found_index);

                            res_args[found_index] = std::get<1>(arg);
                            res_args[found_index]->SetIsPassByRef(is_ref);
                            res_args[found_index]->SetIsPassConst(is_const);
                        }
                    } else if (found_index != -1) {
                        // store used index
                        used_indices.insert(found_index);

                        const GenericInstanceTypeInfo::Arg &param = (found_index < generic_args.Size())
                            ? generic_args[found_index]
                            : generic_args.Back();

                        // CheckArgTypeCompatible(
                        //     visitor,
                        //     std::get<1>(arg)->GetLocation(),
                        //     std::get<0>(arg).type,
                        //     param.m_type
                        // );

                        AssertThrow(found_index < res_args.Size());
                        res_args[found_index] = std::get<1>(arg);
                        res_args[found_index]->SetIsPassByRef(param.m_is_ref);
                        res_args[found_index]->SetIsPassConst(param.m_is_const);
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
                            generic_args[unused_index].m_name,
                            location
                        ));
                    }
                    
                    // push the default value as argument
                    // use named argument, same name as in definition

                    substituted_arg->SetIsPassByRef(is_ref);
                    substituted_arg->SetIsPassConst(is_const);

                    substituted_arg->Visit(visitor, mod);

                    ArgInfo arg_info;
                    arg_info.is_named = substituted_arg->IsNamed();
                    arg_info.name = substituted_arg->GetName();
                    arg_info.type = substituted_arg->GetExprType();

                    const int found_index = ArgIndex(
                        unused_index_counter,
                        arg_info,
                        used_indices,
                        generic_args
                    );

                    if (found_index == -1) {
                        res_args.PushBack(std::move(substituted_arg));
                    } else {
                        AssertThrow(found_index < res_args.Size());

                        res_args[found_index] = std::move(substituted_arg);
                    }
                
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

            return res_args;
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
    const SourceLocation &location
)
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

        if (assignment_type->IsAnyType()) {
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
