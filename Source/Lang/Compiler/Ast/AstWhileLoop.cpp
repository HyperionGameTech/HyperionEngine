#include <Lang/Compiler/Ast/AstWhileLoop.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Emit/Instruction.hpp>
#include <Lang/Compiler/Emit/StaticObject.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/Unicode.hpp>

#include <sstream>

#include <AstWhileLoop.generated.inl>

namespace Hyperion {

AstWhileLoop::AstWhileLoop(const SharedPtr<AstExpression>& conditional,
    const SharedPtr<AstBlock>& block,
    const SourceLocation& location)
    : AstStatement(location),
      m_conditional(conditional),
      m_block(block),
      m_numLocals(0)
{
}

void AstWhileLoop::Visit(AstVisitor* visitor, Module* mod)
{
    // open scope
    mod->scopeTree.Open(SCOPE_TYPE_LOOP);

    // visit the conditional
    m_conditional->Visit(visitor, mod);

    // visit the body
    m_block->Visit(visitor, mod);

    Scope& thisScope = mod->scopeTree.Top();
    m_numLocals = thisScope.identifierTable.CountUsedVariables();

    // close scope
    mod->scopeTree.Close();
}

UniquePtr<Buildable> AstWhileLoop::Build(AstVisitor* visitor, Module* mod)
{
    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_LOOP);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const Tribool conditionIsTrue = m_conditional->IsTrue();

    if (conditionIsTrue == TRI_INDETERMINATE)
    {
        // the condition cannot be determined at compile time
        uint8 rp;

        LabelId topLabel = contextGuard->NewLabel(HYP_NAME(LoopTopLabel));
        chunk->TakeOwnershipOfLabel(topLabel);

        // the label to jump to the end to BREAK
        LabelId breakLabel = contextGuard->NewLabel(HYP_NAME(LoopBreakLabel));
        chunk->TakeOwnershipOfLabel(breakLabel);

        // the label to jump to the end to CONTINUE
        LabelId continueLabel = contextGuard->NewLabel(HYP_NAME(LoopContinueLabel));
        chunk->TakeOwnershipOfLabel(continueLabel);

        // get current register index
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // where to jump up to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(topLabel));

        // build the conditional
        chunk->Append(m_conditional->Build(visitor, mod));

        // compare the conditional to 0
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

        // break away if the condition is false (equal to zero)
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, breakLabel));

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // where 'continue' jumps to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(continueLabel));

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
    }
    else if (conditionIsTrue == TRI_TRUE)
    {
        LabelId topLabel = contextGuard->NewLabel(HYP_NAME(LoopTopLabel));
        chunk->TakeOwnershipOfLabel(topLabel);

        // the label to jump to the end to BREAK
        LabelId breakLabel = contextGuard->NewLabel(HYP_NAME(LoopBreakLabel));
        chunk->TakeOwnershipOfLabel(breakLabel);

        // the label to jump to the end to CONTINUE
        LabelId continueLabel = contextGuard->NewLabel(HYP_NAME(LoopContinueLabel));
        chunk->TakeOwnershipOfLabel(continueLabel);

        chunk->Append(BytecodeUtil::Make<LabelMarker>(topLabel));

        // the condition has been determined to be true
        if (m_conditional->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // where 'continue' jumps to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(continueLabel));

        // pop all local variables off the stack
        for (int i = 0; i < m_numLocals; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_numLocals));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, topLabel));

        // Add break label after the top label
        chunk->Append(BytecodeUtil::Make<LabelMarker>(breakLabel));
    }
    else
    { // false
        // the condition has been determined to be false
        if (m_conditional->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));

            // pop all local variables off the stack
            for (int i = 0; i < m_numLocals; i++)
            {
                visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
            }

            chunk->Append(Compiler::PopStack(visitor, m_numLocals));
        }
    }

    return chunk;
}

void AstWhileLoop::Optimize(AstVisitor* visitor, Module* mod)
{
    // optimize the conditional
    m_conditional->Optimize(visitor, mod);
    // optimize the body
    m_block->Optimize(visitor, mod);
}

SharedPtr<AstStatement> AstWhileLoop::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
