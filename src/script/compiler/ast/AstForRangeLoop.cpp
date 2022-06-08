#include <script/compiler/ast/AstForRangeLoop.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstUnaryExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>
#include <util/utf8.hpp>

#include <sstream>

namespace hyperion {
namespace compiler {

AstForRangeLoop::AstForRangeLoop(
    const std::shared_ptr<AstVariableDeclaration> &decl,
    const std::shared_ptr<AstExpression> &expr,
    const std::shared_ptr<AstBlock> &block,
    const SourceLocation &location)
    : AstStatement(location),
      m_decl(decl),
      m_expr(expr),
      m_block(block),
      m_num_locals(0)
{
}

void AstForRangeLoop::Visit(AstVisitor *visitor, Module *mod)
{
    // open scope for begin, end vars
    mod->m_scopes.Open(Scope());

    m_end_decl.reset(new AstVariableDeclaration(
        "$__end",
        nullptr,
        std::make_shared<AstCallExpression>( // set it to the result of expr.End()
            std::make_shared<AstMember>("End", CloneAstNode(m_expr), m_location),
            std::vector<std::shared_ptr<AstArgument>>{},
            true, // insert self into call to End()
            m_location
        ), 
        IdentifierFlags::FLAG_CONST, // no need to modify End()
        m_location
    ));

    m_end_decl->Visit(visitor, mod);

    m_decl->SetAssignment(std::make_shared<AstCallExpression>( // set it to the result of expr.End()
        std::make_shared<AstMember>("Begin", CloneAstNode(m_expr), m_location),
        std::vector<std::shared_ptr<AstArgument>>{},
        true, // insert self into call to Begin()
        m_location
    )); //  set to expr.Begin() initially. Parser should ensure it is set to nullptr.

    m_decl->Visit(visitor, mod);
    
    // compare <var> != $__end
    m_conditional.reset(new AstBinaryExpression(
        std::make_shared<AstVariable>(m_decl->GetName(), m_location),
        std::make_shared<AstVariable>("$__end", m_location),
        Operator::FindBinaryOperator(Operators::OP_not_eql),
        m_location
    ));

    // <var>++
    m_inc_expr.reset(new AstUnaryExpression(
        std::make_shared<AstVariable>(m_decl->GetName(), m_location),
        Operator::FindUnaryOperator(Operators::OP_increment),
        m_location
    ));

    // open main loop scope
    mod->m_scopes.Open(Scope(SCOPE_TYPE_LOOP, 0));

    // visit the conditional
    m_conditional->Visit(visitor, mod);

    // visit the body
    m_block->Visit(visitor, mod);

    // visit the increment expr
    m_inc_expr->Visit(visitor, mod);

    Scope &this_scope = mod->m_scopes.Top();
    m_num_locals = this_scope.GetIdentifierTable().CountUsedVariables();

    // close scope
    mod->m_scopes.Close();

    // close begin, end scope
    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstForRangeLoop::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    AssertThrow(m_decl != nullptr);
    AssertThrow(m_decl->GetAssignment() != nullptr);
    AssertThrow(m_end_decl != nullptr);
    AssertThrow(m_end_decl->GetAssignment() != nullptr);
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_inc_expr != nullptr);

    int condition_is_true = m_conditional->IsTrue();
    if (condition_is_true == -1) {
        // the condition cannot be determined at compile time
        uint8_t rp;

        LabelId top_label = chunk->NewLabel();

        // the label to jump to the end to BREAK
        LabelId break_label = chunk->NewLabel();

        // get current register index
        rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

        // build our memory storage for i, end
        chunk->Append(m_decl->Build(visitor, mod));
        chunk->Append(m_end_decl->Build(visitor, mod));

        // where to jump up to
        chunk->Append(BytecodeUtil::Make<LabelMarker>(top_label));

        // build the conditional
        chunk->Append(m_conditional->Build(visitor, mod));

        // compare the conditional to 0
        chunk->Append(BytecodeUtil::Make<Comparison>(Comparison::CMPZ, rp));

        // break away if the condition is false (equal to zero)
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JE, break_label));

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // pop all local variables off the stack
        for (int i = 0; i < m_num_locals; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_locals));

        // increment the iterator
        chunk->Append(m_inc_expr->Build(visitor, mod));

        // jump back to top here
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, top_label));

        // set the label's position to after the block,
        // so we can skip it if the condition is false
        chunk->Append(BytecodeUtil::Make<LabelMarker>(break_label));
    } else if (condition_is_true) {
        LabelId top_label = chunk->NewLabel();
        chunk->Append(BytecodeUtil::Make<LabelMarker>(top_label));

        // the condition has been determined to be true
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }

        // enter the block
        chunk->Append(m_block->Build(visitor, mod));

        // pop all local variables off the stack
        for (int i = 0; i < m_num_locals; i++) {
            visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
        }

        chunk->Append(Compiler::PopStack(visitor, m_num_locals));

        // only increment iterator if the call may have side effects
        if (m_inc_expr->MayHaveSideEffects()) {
            chunk->Append(m_inc_expr->Build(visitor, mod));
        }

        // jump back to top here (loop infinitely)
        chunk->Append(BytecodeUtil::Make<Jump>(Jump::JMP, top_label));
    } else {
        // the condition has been determined to be false
        if (m_conditional->MayHaveSideEffects()) {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));

            // pop all local variables off the stack
            for (int i = 0; i < m_num_locals; i++) {
                visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
            }

            chunk->Append(Compiler::PopStack(visitor, m_num_locals));
        }
    }

    return std::move(chunk);
}

void AstForRangeLoop::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_decl != nullptr);
    AssertThrow(m_decl->GetAssignment() != nullptr);
    AssertThrow(m_end_decl != nullptr);
    AssertThrow(m_end_decl->GetAssignment() != nullptr);
    AssertThrow(m_conditional != nullptr);
    AssertThrow(m_inc_expr != nullptr);

    m_decl->Optimize(visitor, mod);
    m_end_decl->Optimize(visitor, mod);
    m_inc_expr->Optimize(visitor, mod);

    // optimize the conditional
    m_conditional->Optimize(visitor, mod);
    // optimize the body
    m_block->Optimize(visitor, mod);
}

Pointer<AstStatement> AstForRangeLoop::Clone() const
{
    return CloneImpl();
}

} // namespace compiler
} // namespace hyperion
