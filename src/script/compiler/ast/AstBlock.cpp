#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <limits>

namespace hyperion {

AstBlock::AstBlock(
    const Array<RC<AstStatement>>& children,
    const SourceLocation& location)
    : AstStatement(location),
      m_children(children),
      m_numLocals(0),
      m_lastIsReturn(false),
      m_lastIsExpr(false)
{
}

AstBlock::AstBlock(const SourceLocation& location)
    : AstStatement(location),
      m_numLocals(0),
      m_lastIsReturn(false),
      m_lastIsExpr(false)
{
}

void AstBlock::Visit(AstVisitor* visitor, Module* mod)
{
    // open the new scope
    mod->m_scopes.Open(m_scopeType, m_scopeFlags);
    m_scope = &mod->m_scopes.Top();

    // visit all children in the block
    for (RC<AstStatement>& child : m_children)
    {
        Assert(child != nullptr);

        child->Visit(visitor, mod);
    }

    m_lastIsReturn = m_children.Any() && (dynamic_cast<AstReturnStatement*>(m_children.Back().Get()) != nullptr);
    m_lastIsExpr = m_children.Any() && (dynamic_cast<AstExpression*>(m_children.Back().Get()) != nullptr);

    if (m_lastIsExpr)
    {
        AstExpression* expr = static_cast<AstExpression*>(m_children.Back().Get());
        Assert(expr != nullptr);

        m_lastExprType = expr->GetExprType();
        Assert(m_lastExprType != nullptr);

        m_lastExprType = m_lastExprType->GetUnaliased();
    }

    // store number of locals, so we can pop them from the stack later
    m_numLocals = m_scope->identifierTable.CountUsedVariables();

    // go down to previous scope
    mod->m_scopes.Close();
}

UniquePtr<Buildable> AstBlock::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const int stackSizeBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    for (const RC<AstStatement>& stmt : m_children)
    {
        Assert(stmt != nullptr);

        chunk->Append(stmt->Build(visitor, mod));
    }

    if (m_lastIsExpr)
    {
        // Deref if needed so we don't leave any dangling reference in the register after the block.
        chunk->Append(Compiler::DerefIfNeeded(visitor, mod, m_lastExprType));
    }

    const int stackSizeNow = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();
    const int stackSizeDiff = stackSizeNow - stackSizeBefore;

    Assert(stackSizeDiff >= 0);

    if (stackSizeDiff > 0)
    {
        visitor->GetCompilationUnit()->GetInstructionStream().SetStackSize(stackSizeBefore);

        chunk->Append(Compiler::PopStack(visitor, stackSizeNow - stackSizeBefore));
    }

    return chunk;
}

void AstBlock::Optimize(AstVisitor* visitor, Module* mod)
{
    for (auto& child : m_children)
    {
        if (child)
        {
            child->Optimize(visitor, mod);
        }
    }
}

RC<AstStatement> AstBlock::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion
