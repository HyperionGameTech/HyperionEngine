#include <Lang/Compiler/Ast/AstContinueStatement.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Configuration.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <AstContinueStatement.generated.inl>

namespace Hyperion {

AstContinueStatement::AstContinueStatement(const SourceLocation& location)
    : AstStatement(location),
      m_numPops(0)
{
}

void AstContinueStatement::Visit(AstVisitor* visitor, Module* mod)
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
            Msg_continue_outside_loop,
            m_location));
    }
}

UniquePtr<Buildable> AstContinueStatement::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const auto* closestLoop = visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree().FindClosestMatch(
        [](const TreeNode<InstructionStreamContext>*, const InstructionStreamContext& context)
        {
            return context.GetType() == INSTRUCTION_STREAM_CONTEXT_LOOP;
        });

    Assert(closestLoop != nullptr, "No loop context found");

    const Optional<LabelId> labelId = closestLoop->FindLabelByName(HYP_NAME(LoopContinueLabel));
    Assert(labelId.HasValue(), "Continue label not found in loop context");

    chunk->Append(BytecodeUtil::Make<Comment>("Skip to next iteration in loop"));

    chunk->Append(Compiler::PopStack(visitor, m_numPops));
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, labelId.Get()));

    return chunk;
}

void AstContinueStatement::Optimize(AstVisitor* visitor, Module* mod)
{
}

Handle<AstStatement> AstContinueStatement::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
