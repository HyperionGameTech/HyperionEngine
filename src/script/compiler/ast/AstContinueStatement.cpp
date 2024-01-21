#include <script/compiler/ast/AstContinueStatement.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstContinueStatement::AstContinueStatement(const SourceLocation &location)
    : AstStatement(location),
      m_num_pops(0)
{
}

void AstContinueStatement::Visit(AstVisitor *visitor, Module *mod)
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
            Msg_continue_outside_loop,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstContinueStatement::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const auto *closest_loop = visitor->GetCompilationUnit()->GetInstructionStream().GetContextTree().FindClosestMatch(
        [](const TreeNode<InstructionStreamContext> *, const InstructionStreamContext &context)
        {
            return context.GetType() == INSTRUCTION_STREAM_CONTEXT_LOOP;
        }
    );

    AssertThrowMsg(closest_loop != nullptr, "No loop context found");

    const Optional<LabelId> label_id = closest_loop->FindLabelByName(HYP_NAME(LoopContinueLabel));
    AssertThrowMsg(label_id.HasValue(), "Continue label not found in loop context");

    chunk->Append(BytecodeUtil::Make<Comment>("Skip to next iteration in loop"));

    chunk->Append(Compiler::PopStack(visitor, m_num_pops));
    chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, label_id.Get()));

    return chunk;
}

void AstContinueStatement::Optimize(AstVisitor *visitor, Module *mod)
{
}

RC<AstStatement> AstContinueStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
