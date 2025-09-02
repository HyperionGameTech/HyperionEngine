#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <limits>

namespace hyperion::compiler {

AstBlock::AstBlock(const Array<RC<AstStatement>>& children,
    const SourceLocation& location)
    : AstStatement(location),
      m_children(children),
      m_numLocals(0),
      m_lastIsReturn(false)
{
}

AstBlock::AstBlock(const SourceLocation& location)
    : AstStatement(location),
      m_numLocals(0),
      m_lastIsReturn(false)
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

    // store number of locals, so we can pop them from the stack later
    m_numLocals = m_scope->identifierTable.CountUsedVariables();

    // go down to previous scope
    mod->m_scopes.Close();
}

HYP_DISABLE_OPTIMIZATION;
UniquePtr<Buildable> AstBlock::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const int stackSizeBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    for (RC<AstStatement>& stmt : m_children)
    {
        Assert(stmt != nullptr);

        chunk->Append(stmt->Build(visitor, mod));
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
HYP_ENABLE_OPTIMIZATION;

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

} // namespace hyperion::compiler
