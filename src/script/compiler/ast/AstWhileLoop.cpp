#include <script/compiler/ast/AstWhileLoop.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <util/Utf8.hpp>

#include <sstream>

namespace hyperion::compiler {

AstWhileLoop::AstWhileLoop(const std::shared_ptr<AstExpression> &conditional,
    const std::shared_ptr<AstBlock> &block,
    const SourceLocation &location)
    : AstStatement(location),
      m_conditional(conditional),
      m_block(block),
      m_num_locals(0)
{
}

void AstWhileLoop::Visit(AstVisitor *visitor, Module *mod)
{
    // open scope
    mod->m_scopes.Open(Scope(SCOPE_TYPE_LOOP, 0));

    // visit the conditional
    m_conditional->Visit(visitor, mod);

    // visit the body
    m_block->Visit(visitor, mod);

    Scope &this_scope = mod->m_scopes.Top();
    m_num_locals = this_scope.GetIdentifierTable().CountUsedVariables();

    // close scope
    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstWhileLoop::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    int condition_is_true = m_conditional->IsTrue();
    if (condition_is_true == -1) {
        // the condition cannot be determined at compile time
        uint8_t rp;

        LabelId top_label = chunk->NewLabel();

        // the label to jump to the end to BREAK
        LabelId break_label = chunk->NewLabel();

        // get current register index
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // where to jump up to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(top_label));

        // build the conditional
        chunk->Append(m_conditional->Build(visitor, mod));

        // compare the conditional to 0
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

        // break away if the condition is false (equal to zero)
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, break_label));

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // pop all local variables off the stack
        for (int i = 0; i < m_num_locals; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_locals));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, top_label));

        // set the label's position to after the block,
        // so we can skip it if the condition is false
        chunk->Append(BytecodeUtil::Make<LabelMarker>(break_label));
    } else if (condition_is_true) {
        LabelId top_label = chunk->NewLabel();
        chunk->Append(BytecodeUtil::Make<LabelMarker>(top_label));

        // the condition has been determined to be true
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // pop all local variables off the stack
        for (int i = 0; i < m_num_locals; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_locals));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, top_label));
    } else {
        // the condition has been determined to be false
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));

            // pop all local variables off the stack
            for (int i = 0; i < m_num_locals; i++) {
                visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
            }

            chunk->Append(Compiler::PopStack(visitor, m_num_locals));
        }
    }

    return std::move(chunk);
}

void AstWhileLoop::Optimize(AstVisitor *visitor, Module *mod)
{
    // optimize the conditional
    m_conditional->Optimize(visitor, mod);
    // optimize the body
    m_block->Optimize(visitor, mod);
}

Pointer<AstStatement> AstWhileLoop::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
