#include <Lang/Compiler/Ast/AstForLoop.hpp>
#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Ast/AstArgument.hpp>
#include <Lang/Compiler/Emit/Instruction.hpp>
#include <Lang/Compiler/Emit/StaticObject.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>
#include <Core/Unicode.hpp>

namespace Hyperion {

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
    mod->scopeTree.Open(SCOPE_TYPE_LOOP);

    if (m_declPart != nullptr)
    {
        m_declPart->Visit(visitor, mod);
    }

    // visit the conditional
    m_conditionPart->Visit(visitor, mod);

    mod->scopeTree.Open(SCOPE_TYPE_LOOP);

    // visit the body
    m_block->Visit(visitor, mod);

    m_numLocals = mod->scopeTree.Top().identifierTable.CountUsedVariables();

    // close variable decl scope
    mod->scopeTree.Close();

    if (m_incrementPart != nullptr)
    {
        m_incrementPart->Visit(visitor, mod);
    }

    m_numUsedInitializers = mod->scopeTree.Top().identifierTable.CountUsedVariables();

    // close scope
    mod->scopeTree.Close();
}

UniquePtr<Buildable> AstForLoop::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_conditionPart != nullptr);

    InstructionStreamContextGuard contextGuard(
        &visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree(),
        INSTRUCTION_STREAM_CONTEXT_LOOP);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    chunk->Append(BytecodeUtil::Make<Comment>("Begin for loop: " + ToString()));

    int conditionIsTrue = m_conditionPart->IsTrue();

    if (conditionIsTrue == -1)
    {
        chunk->Append(BytecodeUtil::Make<Comment>("Runtime condition evaluation for for loop"));
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
            chunk->Append(BytecodeUtil::Make<Comment>("Executing for loop initializer"));
            chunk->Append(m_declPart->Build(visitor, mod));
        }

        // where to jump up to
        chunk->Append(BytecodeUtil::Make<Comment>("Loop condition evaluation point"));
        chunk->Append(BytecodeUtil::Make<LabelMarker>(topLabel));

        // build the conditional
        chunk->Append(BytecodeUtil::Make<Comment>("Evaluating for loop condition"));
        chunk->Append(m_conditionPart->Build(visitor, mod));

        // compare the conditional to 0
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

        // break away if the condition is false (equal to zero)
        chunk->Append(BytecodeUtil::Make<Comment>("Break from loop if condition is false"));
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, breakLabel));

        // enter the block
        chunk->Append(BytecodeUtil::Make<Comment>("Executing for loop body"));
        chunk->Append(m_block->Build(visitor, mod));

        // where 'continue' jumps to
        chunk->Append(BytecodeUtil::Make<Comment>("Continue target - execute increment expression"));
        chunk->Append(BytecodeUtil::Make<LabelMarker>(continueLabel));

        if (m_incrementPart != nullptr)
        {
            chunk->Append(BytecodeUtil::Make<Comment>("Executing for loop increment"));
            chunk->Append(m_incrementPart->Build(visitor, mod));
        }

        // pop all local variables off the stack
        chunk->Append(BytecodeUtil::Make<Comment>("Cleaning up " + String::ToString(m_numLocals) + " local variables"));
        for (int i = 0; i < m_numLocals; i++)
        {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_numLocals));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Comment>("Jump back to condition evaluation"));
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, topLabel));

        // set the label's position to after the block,
        // so we can skip it if the condition is false
        chunk->Append(BytecodeUtil::Make<Comment>("Break target - exit for loop"));
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

String AstForLoop::ToString() const
{
    String result = "for (";

    if (m_declPart)
    {
        result += m_declPart->ToString();
    }
    result += "; ";

    if (m_conditionPart)
    {
        result += m_conditionPart->ToString();
    }
    result += "; ";

    if (m_incrementPart)
    {
        result += m_incrementPart->ToString();
    }
    result += ") { ... }";

    return result;
}

} // namespace Hyperion
