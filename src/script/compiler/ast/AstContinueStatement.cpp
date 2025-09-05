#include <script/compiler/ast/AstContinueStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

namespace hyperion {

AstContinueStatement::AstContinueStatement(const SourceLocation& location)
    : AstStatement(location),
      m_numPops(0)
{
}

void AstContinueStatement::Visit(AstVisitor* visitor, Module* mod)
{
    bool inLoop = false;

    TreeNode<Scope>* top = mod->m_scopes.TopNode();

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

RC<AstStatement> AstContinueStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion
