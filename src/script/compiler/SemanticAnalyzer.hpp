#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/enums.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <memory>
#include <vector>
#include <string>
#include <utility>

namespace hyperion {
namespace compiler {

// forward declaration
class Module;

class SemanticAnalyzer : public AstVisitor {
public:
    struct Helpers {
        struct IdentifierLookupResult {
            Identifier *as_identifier = nullptr;
            Module *as_module = nullptr;
            SymbolTypePtr_t as_type;

            IdentifierType type;
        };

        static IdentifierLookupResult LookupIdentifier(AstVisitor *visitor, Module *mod, const std::string &name);

        static std::pair<SymbolTypePtr_t, std::vector<std::shared_ptr<AstArgument>>>
        SubstituteFunctionArgs(
            AstVisitor *visitor,
            Module *mod, 
            const SymbolTypePtr_t &identifier_type, 
            const std::vector<std::shared_ptr<AstArgument>> &args,
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

    // virtual void Visit(AstActionExpression *);
    // virtual void Visit(AstAliasDeclaration *);
    // virtual void Visit(AstArgument *);
    // virtual void Visit(AstArgumentList *);
    // virtual void Visit(AstArrayAccess *);
    // virtual void Visit(AstArrayExpression *);
    // virtual void Visit(AstBinaryExpression *);
    // virtual void Visit(AstBlock *);
    // virtual void Visit(AstCallExpression *);
    // virtual void Visit(AstDeclaration *);
    // virtual void Visit(AstDirective *);
    // virtual void Visit(AstEvent *);
    // virtual void Visit(AstFalse *);
    // virtual void Visit(AstFileImport *);
    // virtual void Visit(AstFloat *);
    // virtual void Visit(AstFunctionDefinition *);
    // virtual void Visit(AstFunctionExpression *);
    // virtual void Visit(AstHasExpression *);
    // virtual void Visit(AstIdentifier *);
    // virtual void Visit(AstIfStatement *);
    // virtual void Visit(AstImport *);
    // virtual void Visit(AstInteger *);
    // virtual void Visit(AstMember *);
    // virtual void Visit(AstModuleAccess *);
    // virtual void Visit(AstModuleDeclaration *);
    // virtual void Visit(AstModuleImport *);
    // virtual void Visit(AstModuleProperty *);
    // virtual void Visit(AstNewExpression *);
    // virtual void Visit(AstNil *);
    // virtual void Visit(AstNodeBuilder *);
    // virtual void Visit(AstObject *);
    // virtual void Visit(AstParameter *);
    // virtual void Visit(AstPrintStatement *);
    // virtual void Visit(AstReturnStatement *);
    // virtual void Visit(AstSelf *);
    // virtual void Visit(AstString *);
    // virtual void Visit(AstTrue *);
    // virtual void Visit(AstTryCatch *);
    // virtual void Visit(AstTupleExpression *);
    // virtual void Visit(AstTypeAlias *);
    // virtual void Visit(AstTypeDefinition *);
    // virtual void Visit(AstTypeOfExpression *);
    // virtual void Visit(AstTypeSpecification *);
    // virtual void Visit(AstUnaryExpression *);
    // virtual void Visit(AstUndefined *);
    // virtual void Visit(AstVariable *);
    // virtual void Visit(AstVariableDeclaration *);
    // virtual void Visit(AstWhileLoop *);
    // virtual void Visit(AstYieldStatement *);
};

} // namespace compiler
} // namespace hyperion

#endif
