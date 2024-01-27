#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <core/lib/Optional.hpp>

#include <memory>
#include <utility>

namespace hyperion::compiler {

// forward declaration
class Module;

struct SubstitutionResult {
    RC<AstArgument> arg;
    SizeType        index = SizeType(-1);
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
        static SizeType FindFreeSlot(
            SizeType current_index,
            const FlatSet<SizeType> &used_indices,
            const Array<GenericInstanceTypeInfo::Arg> &generic_args,
            Bool is_variadic,
            SizeType num_supplied_args
        );

        static SizeType ArgIndex(
            SizeType current_index,
            const ArgInfo &arg_info,
            const FlatSet<SizeType> &used_indices,
            const Array<GenericInstanceTypeInfo::Arg> &generic_args,
            Bool is_variadic = false,
            SizeType num_supplied_args = -1
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

        static Optional<SymbolTypeFunctionSignature> ExtractGenericArgs(
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

        static Optional<SymbolTypeFunctionSignature> SubstituteFunctionArgs(
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
