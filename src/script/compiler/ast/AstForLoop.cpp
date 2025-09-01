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
#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion::compiler {

AstForLoop::AstForLoop(
    const RC<AstStatement>& declPart,
    const RC<AstExpression>& conditionPart,
    const RC<AstExpression>& incrementPart,
    const RC<AstBlock>& block,
    const SourceLocation& location)
    : AstStatement(location),
      m_declPart(declPart),
      m_conditionPart(conditionPart),
      m_incrementPart(incrementPart),
      m_block(block)
{
}

void AstForLoop::Visit(AstVisitor* visitor, Module* mod)
{
    // if no condition has been provided, replace it with AstTrue
    if (m_conditionPart == nullptr)
    {
        m_incrementPart.Reset(new AstTrue(m_location));
    }

    // open scope for variable decl
    mod->m_scopes.Open(SCOPE_TYPE_LOOP);

    if (m_declPart != nullptr)
    {
        m_declPart->Visit(visitor, mod);
    }

    // visit the conditional
    m_conditionPart->Visit(visitor, mod);

    mod->m_scopes.Open(SCOPE_TYPE_LOOP);

    // visit the body
    m_block->Visit(visitor, mod);

    m_numLocals = mod->m_scopes.Top().identifierTable.CountUsedVariables();

    // close variable decl scope
    mod->m_scopes.Close();

    if (m_incrementPart != nullptr)
    {
        m_incrementPart->Visit(visitor, mod);
    }

    m_numUsedInitializers = mod->m_scopes.Top().identifierTable.CountUsedVariables();

    // close scope
    mod->m_scopes.Close();
}

UniquePtr<Buildable> AstForLoop::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_conditionPart != nullptr);

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_LOOP);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    int conditionIsTrue = m_conditionPart->IsTrue();

    if (conditionIsTrue == -1)
    {
        // the condition cannot be determined at compile time
        uint8 rp;

        LabelId topLabel = contextGuard->NewLabel(HYP_NAME(LoopTopLabel));
        chunk->TakeOwnershipOfLabel(topLabel);

        // the label to jump to the end for BREAK
        LabelId breakLabel = contextGuard->NewLabel(HYP_NAME(LoopBreakLabel));
        chunk->TakeOwnershipOfLabel(breakLabel);

        // the label to jump to on CONTINUE
        LabelId continueLabel = contextGuard->NewLabel(HYP_NAME(LoopContinueLabel));
        chunk->TakeOwnershipOfLabel(continueLabel);

        // get current register index
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // initializers
        if (m_declPart != nullptr)
        {
            chunk->Append(m_declPart->Build(visitor, mod));
        }

        // where to jump up to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(topLabel));

        // build the conditional
        chunk->Append(m_conditionPart->Build(visitor, mod));

        // compare the conditional to 0
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

        // break away if the condition is false (equal to zero)
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, breakLabel));

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // where 'continue' jumps to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(continueLabel));

        if (m_incrementPart != nullptr)
        {
            chunk->Append(m_incrementPart->Build(visitor, mod));
        }

        // pop all local variables off the stack
        for (int i = 0; i < m_numLocals; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_numLocals));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, topLabel));

        // set the label's position to after the block,
        // so we can skip it if the condition is false
        chunk->Append(BytecodeUtil::Make<LabelMarker>(breakLabel));

        // pop all initializers off the stack
        for (int i = 0; i < m_numUsedInitializers; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_numUsedInitializers));
    }
    else if (conditionIsTrue)
    {
        if (m_declPart != nullptr)
        {
            chunk->Append(m_declPart->Build(visitor, mod));
        }

        LabelId topLabel = contextGuard->NewLabel(HYP_NAME(LoopTopLabel));

        // the label to jump to the end to BREAK
        LabelId breakLabel = contextGuard->NewLabel(HYP_NAME(LoopBreakLabel));

        // the label to jump to for 'continue' statement
        LabelId continueLabel = contextGuard->NewLabel(HYP_NAME(LoopContinueLabel));
        chunk->TakeOwnershipOfLabel(continueLabel);

        chunk->Append(BytecodeUtil::Make<LabelMarker>(topLabel));

        // the condition has been determined to be true
        if (m_conditionPart->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditionPart->Build(visitor, mod));
        }

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // where 'continue' jumps to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(continueLabel));

        if (m_incrementPart != nullptr)
        {
            chunk->Append(m_incrementPart->Build(visitor, mod));
        }

        // pop all local variables off the stack
        for (int i = 0; i < m_numLocals; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_numLocals));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, topLabel));

        // Break is after the JMP instruction to go back to the top
        chunk->Append(BytecodeUtil::Make<LabelMarker>(breakLabel));

        // pop all initializers off the stack
        for (int i = 0; i < m_numUsedInitializers; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_numUsedInitializers));
    }
    else
    {
        if (m_declPart != nullptr)
        {
            chunk->Append(m_declPart->Build(visitor, mod));
        }

        // the condition has been determined to be false
        if (m_conditionPart->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditionPart->Build(visitor, mod));

            if (m_incrementPart != nullptr)
            {
                chunk->Append(m_incrementPart->Build(visitor, mod));
            }

            // pop all local variables off the stack
            for (int i = 0; i < m_numLocals; i++)
            {
                visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
            }

            chunk->Append(Compiler::PopStack(visitor, m_numLocals));
        }

        // pop all initializers off the stack
        for (int i = 0; i < m_numUsedInitializers; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_numUsedInitializers));
    }

    return chunk;
}

void AstForLoop::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_declPart != nullptr)
    {
        m_declPart->Optimize(visitor, mod);
    }

    if (m_conditionPart != nullptr)
    {
        m_conditionPart->Optimize(visitor, mod);
    }

    if (m_incrementPart != nullptr)
    {
        m_incrementPart->Optimize(visitor, mod);
    }

    if (m_block != nullptr)
    {
        m_block->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstForLoop::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
