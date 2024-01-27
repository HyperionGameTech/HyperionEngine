#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <memory>
#include <utility>
#include <set>

namespace hyperion::compiler {

// forward declaration
class Module;

struct SubstitutionResult {
    RC<AstArgument> arg;
    Int             index = -1;
};

struct ArgInfo
{
    Bool            is_named;
    String          name;
    SymbolTypePtr_t type;
};

class SemanticAnalyzer : public AstVisitor
{
public:
    struct Helpers
    {
    private:
        static Int FindFreeSlot(
            Int current_index,
            const FlatSet<Int> &used_indices,
            const Array<GenericInstanceTypeInfo::Arg> &generic_args,
            Bool is_variadic,
            Int num_supplied_args
        );

        static Int ArgIndex(
            Int current_index,
            const ArgInfo &arg_info,
            const FlatSet<Int> &used_indices,
            const Array<GenericInstanceTypeInfo::Arg> &generic_args,
            Bool is_variadic = false,
            Int num_supplied_args = -1
        );

    public:
        static SymbolTypePtr_t GetVarArgType(
            const Array<GenericInstanceTypeInfo::Arg> &generic_args
        );

        static SymbolTypePtr_t SubstituteGenericParameters(
            AstVisitor *visitor,
            Module *mod,
            const SymbolTypePtr_t &input_type,
            const Array<GenericInstanceTypeInfo::Arg> &generic_args,
            const Array<SubstitutionResult> &substitution_results,
            const SourceLocation &location
        );

        static FunctionTypeSignature_t ExtractGenericArgs(
            AstVisitor *visitor,
            Module *mod,
            const SymbolTypePtr_t &symbol_type, 
            const Array<RC<AstArgument>> &args,
            const SourceLocation &location,
            Array<SubstitutionResult>(*fn) (
                AstVisitor *visitor,
                Module *mod,
                const Array<GenericInstanceTypeInfo::Arg> &generic_args,
                const Array<RC<AstArgument>> &args,
                const SourceLocation &location
            )
        );

        static void CheckArgTypeCompatible(
            AstVisitor *visitor,
            const SourceLocation &location,
            const SymbolTypePtr_t &arg_type,
            const SymbolTypePtr_t &param_type
        );

        static FunctionTypeSignature_t SubstituteFunctionArgs(
            AstVisitor *visitor,
            Module *mod,
            const SymbolTypePtr_t &symbol_type, 
            const Array<RC<AstArgument>> &args,
            const SourceLocation &location
        );

        static void EnsureFunctionArgCompatibility(
            AstVisitor *visitor,
            Module *mod,
            const SymbolTypePtr_t &symbol_type, 
            const Array<RC<AstArgument>> &args,
            const SourceLocation &location
        );

        static void EnsureLooseTypeAssignmentCompatibility(
            AstVisitor *visitor,
            Module *mod,
            const SymbolTypePtr_t &symbol_type,
            const SymbolTypePtr_t &assignment_type,
            const SourceLocation &location
        );

        static void EnsureTypeAssignmentCompatibility(
            AstVisitor *visitor,
            Module *mod,
            const SymbolTypePtr_t &symbol_type,
            const SymbolTypePtr_t &assignment_type,
            const SourceLocation &location
        );
    };

public:
    SemanticAnalyzer(AstIterator *ast_iterator, CompilationUnit *compilation_unit);
    SemanticAnalyzer(const SemanticAnalyzer &other);

    /** Generates the compilation unit structure from the given statement iterator */
    void Analyze(bool expect_module_decl = true);
};

} // namespace hyperion::compiler

#endif
