#include <Lang/Compiler/Ast/AstBreakStatement.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <AstBreakStatement.generated.inl>

namespace Hyperion {

AstBreakStatement::AstBreakStatement(const SourceLocation& location)
    : AstStatement(location),
      m_numPops(0)
{
}

void AstBreakStatement::Visit(AstVisitor* visitor, Module* mod)
{
    bool inLoop = false;

    TreeNode<Scope>* top = mod->scopeTree.TopNode();

    while (top != nullptr)
    {
        m_numPops += top->Get().identifierTable.CountUsedVariables();

        if (top->Get().scopeType == SCOPE_TYPE_LOOP || top->Get().scopeType == SCOPE_TYPE_SWITCH)
        {
            inLoop = true;

            break;
        }

        top = top->m_parent;
    }

    if (!inLoop)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_break_outside_loop,
            m_location));
    }
}

UniquePtr<Buildable> AstBreakStatement::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const auto* closestMatch = visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree().FindClosestMatch(
        [](const TreeNode<InstructionStreamContext>*, const InstructionStreamContext& context)
        {
            return context.GetType() == INSTRUCTION_STREAM_CONTEXT_LOOP ||
                   context.GetType() == INSTRUCTION_STREAM_CONTEXT_SWITCH;
        });

    Assert(closestMatch != nullptr, "No loop or switch context found");

    const bool isSwitchBreak = closestMatch->GetType() == INSTRUCTION_STREAM_CONTEXT_SWITCH;

    const Name breakLabelName = isSwitchBreak
        ? HYP_NAME(SwitchBreakLabel)
        : HYP_NAME(LoopBreakLabel);

    const Optional<LabelId> labelId = closestMatch->FindLabelByName(breakLabelName);
    Assert(labelId.HasValue(), "Break label not found in loop or switch context");

    chunk->Append(BytecodeUtil::Make<Comment>(
        isSwitchBreak ? "Break out of switch" : "Break out of loop"));

    chunk->Append(Compiler::PopStack(visitor, m_numPops));
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, labelId.Get()));

    return chunk;
}

void AstBreakStatement::Optimize(AstVisitor* visitor, Module* mod)
{
}

SharedPtr<AstStatement> AstBreakStatement::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
