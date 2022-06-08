#include <script/compiler/ast/AstIfStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <cstdio>

namespace hyperion {
namespace compiler {

AstIfStatement::AstIfStatement(const std::shared_ptr<AstExpression> &conditional,
    const std::shared_ptr<AstBlock> &block,
    const std::shared_ptr<AstBlock> &else_block,
    const SourceLocation &location)
    : AstStatement(location),
      m_conditional(conditional),
      m_block(block),
      m_else_block(else_block)
{
}

void AstIfStatement::Visit(AstVisitor *visitor, Module *mod)
{
    // visit the conditional
    m_conditional->Visit(visitor, mod);
    // visit the body
    m_block->Visit(visitor, mod);
    // visit the else-block (if it exists)
    if (m_else_block != nullptr) {
        m_else_block->Visit(visitor, mod);
    }
}

std::unique_ptr<Buildable> AstIfStatement::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    int condition_is_true = m_conditional->IsTrue();
    if (condition_is_true == -1) {
        // the condition cannot be determined at compile time
        chunk->Append(Compiler::CreateConditional(
            visitor,
            mod,
            m_conditional.get(),
            m_block.get(),
            m_else_block.get()
        ));
    } else if (condition_is_true) {
        // the condition has been determined to be true
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }
        // enter the block
        chunk->Append(m_block->Build(visitor, mod));
        // do not accept the else-block
    } else {
        // the condition has been determined to be false
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }
        // only visit the else-block (if it exists)
        if (m_else_block != nullptr) {
            chunk->Append(m_else_block->Build(visitor, mod));
        }
    }

    return std::move(chunk);
}

void AstIfStatement::Optimize(AstVisitor *visitor, Module *mod)
{
    // optimize the conditional
    m_conditional->Optimize(visitor, mod);
    // optimize the body
    m_block->Optimize(visitor, mod);
    // optimize the else-block (if it exists)
    if (m_else_block != nullptr) {
        m_else_block->Optimize(visitor, mod);
    }
}

Pointer<AstStatement> AstIfStatement::Clone() const
{
    return CloneImpl();
}

} // namespace compiler
} // namespace hyperion
