#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion {

AstReturnStatement::AstReturnStatement(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstStatement(location),
      m_expr(expr),
      m_numPops(0),
      m_isVisited(false),
      m_isConstructor(false)
{
}

void AstReturnStatement::Visit(AstVisitor* visitor, Module* mod)
{
    if (m_expr != nullptr)
    {
        m_expr->Visit(visitor, mod);
    }

    // transverse the scope tree to make sure we are in a function
    bool inFunction = false;

    TreeNode<Scope>* top = mod->scopeTree.TopNode();

    while (top != nullptr)
    {
        if (top->Get().scopeType == SCOPE_TYPE_FUNCTION)
        {
            inFunction = true;

            if (top->Get().scopeFlags & CONSTRUCTOR_DEFINITION_FLAG)
            {
                m_isConstructor = true;
            }

            break;
        }

        m_numPops += top->Get().identifierTable.CountUsedVariables();
        top = top->m_parent;
    }

    if (inFunction)
    {
        Assert(top != nullptr);

        const SymbolTypeRef& returnType = m_expr ? m_expr->GetExprType() : BuiltinTypes::s_voidType;
        Assert(returnType != nullptr);

        top->Get().returnTypes.PushBack(returnType);

        return;
    }

    // error; 'return' not allowed outside of a function
    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_return_outside_function,
        m_location));
}

UniquePtr<Buildable> AstReturnStatement::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_expr != nullptr)
    {
        chunk->Append(m_expr->Build(visitor, mod));

        chunk->Append(Compiler::DerefIfNeeded(visitor, mod, m_expr->GetExprType()));
    }

    chunk->Append(Compiler::PopStack(visitor, m_numPops));

    // add RET instruction
    auto instrRet = BytecodeUtil::Make<RawOperation<>>();
    instrRet->opcode = RET;
    chunk->Append(std::move(instrRet));

    return chunk;
}

void AstReturnStatement::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_expr != nullptr)
    {
        m_expr->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstReturnStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion
