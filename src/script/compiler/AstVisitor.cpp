#include <script/compiler/AstVisitor.hpp>

namespace hyperion::compiler {

AstVisitor::AstVisitor(AstIterator *ast_iterator,
    CompilationUnit *compilation_unit)
    : m_ast_iterator(ast_iterator),
      m_compilation_unit(compilation_unit)
{
}

bool AstVisitor::Assert(bool expr, const CompilerError &error)
{
    if (!expr) {
        m_compilation_unit->GetErrorList().AddError(error);
        return false;
    }
    return true;
}

// void AstVisitor::Visit(AstStatement *stmt)
// {
//     if (AstActionExpression *node = dynamic_cast<AstActionExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstAliasDeclaration *node = dynamic_cast<AstAliasDeclaration*>(stmt)) {
//         Visit(node);
//     } else if (AstArgument *node = dynamic_cast<AstArgument*>(stmt)) {
//         Visit(node);
//     } else if (AstArgumentList *node = dynamic_cast<AstArgumentList*>(stmt)) {
//         Visit(node);
//     } else if (AstArrayAccess *node = dynamic_cast<AstArrayAccess*>(stmt)) {
//         Visit(node);
//     } else if (AstArrayExpression *node = dynamic_cast<AstArrayExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstBinaryExpression *node = dynamic_cast<AstBinaryExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstBlock *node = dynamic_cast<AstBlock*>(stmt)) {
//         Visit(node);
//     } else if (AstCallExpression *node = dynamic_cast<AstCallExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstDeclaration *node = dynamic_cast<AstDeclaration*>(stmt)) {
//         Visit(node);
//     } else if (AstDirective *node = dynamic_cast<AstDirective*>(stmt)) {
//         Visit(node);
//     } else if (AstEvent *node = dynamic_cast<AstEvent*>(stmt)) {
//         Visit(node);
//     } else if (AstFalse *node = dynamic_cast<AstFalse*>(stmt)) {
//         Visit(node);
//     } else if (AstFileImport *node = dynamic_cast<AstFileImport*>(stmt)) {
//         Visit(node);
//     } else if (AstFloat *node = dynamic_cast<AstFloat*>(stmt)) {
//         Visit(node);
//     } else if (AstFunctionDefinition *node = dynamic_cast<AstFunctionDefinition*>(stmt)) {
//         Visit(node);
//     } else if (AstFunctionExpression *node = dynamic_cast<AstFunctionExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstHasExpression *node = dynamic_cast<AstHasExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstIdentifier *node = dynamic_cast<AstIdentifier*>(stmt)) {
//         Visit(node);
//     } else if (AstIfStatement *node = dynamic_cast<AstIfStatement*>(stmt)) {
//         Visit(node);
//     } else if (AstImport *node = dynamic_cast<AstImport*>(stmt)) {
//         Visit(node);
//     } else if (AstInteger *node = dynamic_cast<AstInteger*>(stmt)) {
//         Visit(node);
//     } else if (AstMember *node = dynamic_cast<AstMember*>(stmt)) {
//         Visit(node);
//     } else if (AstModuleAccess *node = dynamic_cast<AstModuleAccess*>(stmt)) {
//         Visit(node);
//     } else if (AstModuleDeclaration *node = dynamic_cast<AstModuleDeclaration*>(stmt)) {
//         Visit(node);
//     } else if (AstModuleImport *node = dynamic_cast<AstModuleImport*>(stmt)) {
//         Visit(node);
//     } else if (AstModuleProperty *node = dynamic_cast<AstModuleProperty*>(stmt)) {
//         Visit(node);
//     } else if (AstNewExpression *node = dynamic_cast<AstNewExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstNil *node = dynamic_cast<AstNil*>(stmt)) {
//         Visit(node);
//     } else if (AstNodeBuilder *node = dynamic_cast<AstNodeBuilder*>(stmt)) {
//         Visit(node);
//     } else if (AstObject *node = dynamic_cast<AstObject*>(stmt)) {
//         Visit(node);
//     } else if (AstParameter *node = dynamic_cast<AstParameter*>(stmt)) {
//         Visit(node);
//     } else if (AstPrintStatement *node = dynamic_cast<AstPrintStatement*>(stmt)) {
//         Visit(node);
//     } else if (AstReturnStatement *node = dynamic_cast<AstReturnStatement*>(stmt)) {
//         Visit(node);
//     } else if (AstSelf *node = dynamic_cast<AstSelf*>(stmt)) {
//         Visit(node);
//     } else if (AstString *node = dynamic_cast<AstString*>(stmt)) {
//         Visit(node);
//     } else if (AstTrue *node = dynamic_cast<AstTrue*>(stmt)) {
//         Visit(node);
//     } else if (AstTryCatch *node = dynamic_cast<AstTryCatch*>(stmt)) {
//         Visit(node);
//     } else if (AstTupleExpression *node = dynamic_cast<AstTupleExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstTypeAlias *node = dynamic_cast<AstTypeAlias*>(stmt)) {
//         Visit(node);
//     } else if (AstTypeDefinition *node = dynamic_cast<AstTypeDefinition*>(stmt)) {
//         Visit(node);
//     } else if (AstTypeOfExpression *node = dynamic_cast<AstTypeOfExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstTypeSpecification *node = dynamic_cast<AstTypeSpecification*>(stmt)) {
//         Visit(node);
//     } else if (AstUnaryExpression *node = dynamic_cast<AstUnaryExpression*>(stmt)) {
//         Visit(node);
//     } else if (AstUndefined *node = dynamic_cast<AstUndefined*>(stmt)) {
//         Visit(node);
//     } else if (AstVariable *node = dynamic_cast<AstVariable*>(stmt)) {
//         Visit(node);
//     } else if (AstVariableDeclaration *node = dynamic_cast<AstVariableDeclaration*>(stmt)) {
//         Visit(node);
//     } else if (AstWhileLoop *node = dynamic_cast<AstWhileLoop*>(stmt)) {
//         Visit(node);
//     } else if (AstWhileLoop *node = dynamic_cast<AstYieldStatement*>(stmt)) {
//         Visit(node);
//     }
// }

} // namespace hyperion::compiler
