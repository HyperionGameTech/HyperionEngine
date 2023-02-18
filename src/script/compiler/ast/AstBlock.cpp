#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <limits>

namespace hyperion::compiler {

AstBlock::AstBlock(const Array<RC<AstStatement>> &children, 
    const SourceLocation &location)
    : AstStatement(location),
      m_children(children),
      m_num_locals(0),
      m_last_is_return(false)
{
}

AstBlock::AstBlock(const SourceLocation &location)
    : AstStatement(location),
      m_num_locals(0),
      m_last_is_return(false)
{
}

void AstBlock::Visit(AstVisitor *visitor, Module *mod)
{
    // open the new scope
    mod->m_scopes.Open(Scope(m_scope_type, m_scope_flags));
    m_scope = &mod->m_scopes.Top();

    // visit all children in the block
    for (RC<AstStatement> &child : m_children) {
        AssertThrow(child != nullptr);

        child->Visit(visitor, mod);
    }

    m_last_is_return = m_children.Any() &&
        (dynamic_cast<AstReturnStatement *>(m_children.Back().Get()) != nullptr);

    // store number of locals, so we can pop them from the stack later
    m_num_locals = m_scope->GetIdentifierTable().CountUsedVariables();

    // go down to previous scope
    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstBlock::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const Int stack_size_before = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    for (RC<AstStatement> &stmt : m_children) {
        AssertThrow(stmt != nullptr);

        chunk->Append(stmt->Build(visitor, mod));
    }

    // how many times to pop the stack
    SizeType pop_times = 0;

    // pop all local variables off the stack
    for (int i = 0; i < m_num_locals; i++) {
        if (!m_last_is_return) {
            pop_times++;
        }

        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    const Int stack_size_now = visitor->GetCompilationUnit()->GetInstructionStream().GetStackSize();

    AssertThrowMsg(
        stack_size_now == stack_size_before,
        "Stack size mismatch detected! Internal record of stack does not match. (%d != %d)",
        stack_size_now,
        stack_size_before
    );

    chunk->Append(Compiler::PopStack(visitor, pop_times));

    return chunk;
}

void AstBlock::Optimize(AstVisitor *visitor, Module *mod)
{
    for (auto &child : m_children) {
        if (child) {
            child->Optimize(visitor, mod);
        }
    }
}

RC<AstStatement> AstBlock::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
