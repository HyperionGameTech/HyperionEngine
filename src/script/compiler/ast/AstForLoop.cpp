#include <script/compiler/ast/AstForLoop.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

namespace hyperion::compiler {

AstForLoop::AstForLoop(
    const std::shared_ptr<AstStatement> &decl_part,
    const std::shared_ptr<AstExpression> &condition_part,
    const std::shared_ptr<AstExpression> &increment_part,
    const std::shared_ptr<AstBlock> &block,
    const SourceLocation &location)
    : AstStatement(location),
      m_decl_part(decl_part),
      m_condition_part(condition_part),
      m_increment_part(increment_part),
      m_block(block)
{
}

void AstForLoop::Visit(AstVisitor *visitor, Module *mod)
{
    // if no condition has been provided, replace it with AstTrue
    if (m_condition_part == nullptr) {
        m_increment_part.reset(new AstTrue(m_location));
    }

    // open scope for variable decl
    mod->m_scopes.Open(Scope(SCOPE_TYPE_LOOP, 0));

    if (m_decl_part != nullptr) {
        m_decl_part->Visit(visitor, mod);
    }

    // visit the conditional
    m_condition_part->Visit(visitor, mod);

    mod->m_scopes.Open(Scope(SCOPE_TYPE_LOOP, 0));

    // visit the body
    m_block->Visit(visitor, mod);

    m_num_locals = mod->m_scopes.Top().GetIdentifierTable().CountUsedVariables();

    // close variable decl scope
    mod->m_scopes.Close();

    if (m_increment_part != nullptr) {
        m_increment_part->Visit(visitor, mod);
    }

    m_num_used_initializers = mod->m_scopes.Top().GetIdentifierTable().CountUsedVariables();

    // close scope
    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstForLoop::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_condition_part != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    int condition_is_true = m_condition_part->IsTrue();
    if (condition_is_true == -1) {
        // the condition cannot be determined at compile time
        uint8_t rp;

        LabelId top_label = chunk->NewLabel();

        // the label to jump to the end to BREAK
        LabelId break_label = chunk->NewLabel();

        // get current register index
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // initializers
        if (m_decl_part != nullptr) {
            chunk->Append(m_decl_part->Build(visitor, mod));
        }

        // where to jump up to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(top_label));

        // build the conditional
        chunk->Append(m_condition_part->Build(visitor, mod));

        // compare the conditional to 0
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

        // break away if the condition is false (equal to zero)
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, break_label));

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        if (m_increment_part != nullptr) {
            chunk->Append(m_increment_part->Build(visitor, mod));
        }

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

        // pop all initializers off the stack
        for (int i = 0; i < m_num_used_initializers; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_used_initializers));

    } else if (condition_is_true) {
        if (m_decl_part != nullptr) {
            chunk->Append(m_decl_part->Build(visitor, mod));
        }

        LabelId top_label = chunk->NewLabel();
        chunk->Append(BytecodeUtil::Make<LabelMarker>(top_label));

        // the condition has been determined to be true
        if (m_condition_part->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_condition_part->Build(visitor, mod));
        }

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        if (m_increment_part != nullptr) {
            chunk->Append(m_increment_part->Build(visitor, mod));
        }

        // pop all local variables off the stack
        for (int i = 0; i < m_num_locals; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_locals));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, top_label));

        // pop all initializers off the stack
        for (int i = 0; i < m_num_used_initializers; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_used_initializers));
    } else {
        if (m_decl_part != nullptr) {
            chunk->Append(m_decl_part->Build(visitor, mod));
        }

        // the condition has been determined to be false
        if (m_condition_part->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_condition_part->Build(visitor, mod));

            if (m_increment_part != nullptr) {
                chunk->Append(m_increment_part->Build(visitor, mod));
            }

            // pop all local variables off the stack
            for (int i = 0; i < m_num_locals; i++) {
                visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
            }

            chunk->Append(Compiler::PopStack(visitor, m_num_locals));
        }

        // pop all initializers off the stack
        for (int i = 0; i < m_num_used_initializers; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_used_initializers));
    }

    return std::move(chunk);
}

void AstForLoop::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_decl_part != nullptr) {
        m_decl_part->Optimize(visitor, mod);
    }

    if (m_condition_part != nullptr) {
        m_condition_part->Optimize(visitor, mod);
    }

    if (m_increment_part != nullptr) {
        m_increment_part->Optimize(visitor, mod);
    }

    if (m_block != nullptr) {
        m_block->Optimize(visitor, mod);
    }
}

Pointer<AstStatement> AstForLoop::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
