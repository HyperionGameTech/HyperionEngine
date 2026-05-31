#include <Lang/Compiler/Ast/AstBreakStatement.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

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

        if (top->Get().scopeType == SCOPE_TYPE_LOOP)
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

    const auto* closestLoop = visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree().FindClosestMatch(
        [](const TreeNode<InstructionStreamContext>*, const InstructionStreamContext& context)
        {
            return context.GetType() == INSTRUCTION_STREAM_CONTEXT_LOOP;
        });

    Assert(closestLoop != nullptr, "No loop context found");

    const Optional<LabelId> labelId = closestLoop->FindLabelByName(HYP_NAME(LoopBreakLabel));
    Assert(labelId.HasValue(), "Break label not found in loop context");

    chunk->Append(BytecodeUtil::Make<Comment>("Break out of loop"));

    chunk->Append(Compiler::PopStack(visitor, m_numPops));
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, labelId.Get()));

    return chunk;
}

void AstBreakStatement::Optimize(AstVisitor* visitor, Module* mod)
{
}

RC<AstStatement> AstBreakStatement::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
