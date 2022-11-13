#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <memory>
#include <vector>
#include <string>
#include <utility>

namespace hyperion::compiler {

// forward declaration
class Module;

class SemanticAnalyzer : public AstVisitor
{
public:
    struct Helpers
    {
        static std::vector<std::shared_ptr<AstArgument>> SubstituteGenericArgs(
            AstVisitor *visitor,
            Module *mod,
            const std::vector<GenericInstanceTypeInfo::Arg> &generic_args,
            const std::vector<std::shared_ptr<AstArgument>> &args,
            const SourceLocation &location
        );

        static FunctionTypeSignature_t SubstituteFunctionArgs(
            AstVisitor *visitor,
            Module *mod, 
            const SymbolTypePtr_t &identifier_type, 
            const std::vector<std::shared_ptr<AstArgument>> &args,
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
