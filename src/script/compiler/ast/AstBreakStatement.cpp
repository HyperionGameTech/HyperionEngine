#include <script/compiler/ast/AstBreakStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstBreakStatement::AstBreakStatement(const SourceLocation &location)
    : AstStatement(location),
      m_num_pops(0)
{
}

void AstBreakStatement::Visit(AstVisitor *visitor, Module *mod)
{
    Bool in_loop = false;

    TreeNode<Scope> *top = mod->m_scopes.TopNode();

    while (top != nullptr) {
        m_num_pops += top->Get().GetIdentifierTable().CountUsedVariables();

        if (top->Get().GetScopeType() == SCOPE_TYPE_LOOP) {
            in_loop = true;

            break;
        }

        top = top->m_parent;
    }

    if (!in_loop) {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_break_outside_loop,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstBreakStatement::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const auto *closest_loop = visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree().FindClosestMatch(
        [](const TreeNode<InstructionStreamContext> *, const InstructionStreamContext &context)
        {
            return context.GetType() == INSTRUCTION_STREAM_CONTEXT_LOOP;
        }
    );

    AssertThrowMsg(closest_loop != nullptr, "No loop context found");

    const Optional<LabelId> label_id = closest_loop->FindLabelByName(HYP_NAME(LoopBreakLabel));
    AssertThrowMsg(label_id.HasValue(), "Break label not found in loop context");

    chunk->Append(BytecodeUtil::Make<Comment>("Break out of loop"));

    chunk->Append(Compiler::PopStack(visitor, m_num_pops));
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, label_id.Get()));

    return chunk;
}

void AstBreakStatement::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstBreakStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
