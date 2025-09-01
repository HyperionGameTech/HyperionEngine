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

UniquePtr<Buildable> AstBlock::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const int stackSizeBefore = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    for (RC<AstStatement>& stmt : m_children)
    {
        Assert(stmt != nullptr);

        chunk->Append(stmt->Build(visitor, mod));
    }

    // how many times to pop the stack
    SizeType popTimes = 0;

    // pop all local variables off the stack
    for (int i = 0; i < m_numLocals; i++)
    {
        if (!m_lastIsReturn)
        {
            popTimes++;
        }

        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    const int stackSizeNow = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    Assert(
        stackSizeNow == stackSizeBefore,
        "Stack size mismatch detected! Internal record of stack does not match. (%d != %d)",
        stackSizeNow,
        stackSizeBefore);

    chunk->Append(Compiler::PopStack(visitor, popTimes));

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

} // namespace hyperion::compiler
