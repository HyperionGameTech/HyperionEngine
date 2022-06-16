#include <script/compiler/ast/AstBlockExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstAliasDeclaration.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

#include <iostream>

namespace hyperion::compiler {

AstBlockExpression::AstBlockExpression(
    const std::shared_ptr<AstBlock> &block,
    const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_block(block),
      m_num_locals(0),
      m_last_is_return(false)
{
}

void AstBlockExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_block != nullptr);

    // hold a vector of all declarations in the block.
    // at the end of the block, create an Object declaration where all members are
    // assigned to the found declarations (members will also have the same name).
    // note, that they are not assigned to the same expression.
    // it will be assigned to the variable itself, so that expressions are only evaluated once.
    std::vector<SymbolMember_t> member_types;
    std::vector<GenericInstanceTypeInfo::Arg> generic_param_types;
    std::vector<std::tuple<std::string, std::shared_ptr<AstVariable>>> block_member_refs;

    // open the new scope
    mod->m_scopes.Open(Scope());

    m_children.reserve(m_block->GetChildren().size());

    for (auto &child : m_block->GetChildren()) {
        AssertThrow(child != nullptr);

        m_children.push_back(child);

        if (auto *decl = dynamic_cast<AstDeclaration *>(child.get())) {
            // rewrite name of variable.
            // prefix the original with $__ to hide it,
            // and create an alias with the original name pointing to this.<var name>
            const std::string var_name = decl->GetName();

            block_member_refs.push_back(std::make_tuple(
                var_name, // original name
                std::shared_ptr<AstVariable>(new AstVariable(
                    var_name,
                    decl->GetLocation()
                ))
            ));
        }
    }

    for (auto &child : m_children) {
        AssertThrow(child != nullptr);
        child->Visit(visitor, mod);
    }

    std::shared_ptr<AstBlock> closure_block(new AstBlock(m_location));

    // add a Closure object that will be the result of the expression
    for (auto &tup : block_member_refs) {
        closure_block->AddChild(std::get<1>(tup));
    }
    
    // this closure object be the last item in the expression, so it will be returned.
    m_result_closure.reset(new AstFunctionExpression(
        {},
        nullptr,
        closure_block,
        false,
        false,
        false,
        m_location
    ));

    m_result_closure->Visit(visitor, mod);
    
    m_last_is_return = !(m_children.empty()) &&
        (dynamic_cast<AstReturnStatement*>(m_children.back().get()) != nullptr);

    // store number of locals, so we can pop them from the stack later
    Scope &this_scope = mod->m_scopes.Top();
    m_num_locals = this_scope.GetIdentifierTable().CountUsedVariables();

    // go down to previous scope
    mod->m_scopes.Close();
}

std::unique_ptr<Buildable> AstBlockExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_result_closure != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build each child
    for (auto &child : m_children) {
        AssertThrow(child != nullptr);
        chunk->Append(child->Build(visitor, mod));
    }

    chunk->Append(m_result_closure->Build(visitor, mod));

    // how many times to pop the stack
    size_t pop_times = 0;

    // pop all local variables off the stack
    for (int i = 0; i < m_num_locals; i++) {
        if (!m_last_is_return) {
            pop_times++;
        }

        visitor->GetCompilationUnit()->GetInstructionStream().DecStackSize();
    }

    chunk->Append(Compiler::PopStack(visitor, pop_times));

    return std::move(chunk);
}

void AstBlockExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_result_closure != nullptr);

    for (auto &child : m_children) {
        AssertThrow(child != nullptr);
        child->Optimize(visitor, mod);
    }

    m_result_closure->Optimize(visitor, mod);
}

Pointer<AstStatement> AstBlockExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstBlockExpression::IsTrue() const
{
    return Tribool::True();
}

bool AstBlockExpression::MayHaveSideEffects() const
{
    for (const auto &child : m_children) {
        if (AstExpression *expr = dynamic_cast<AstExpression*>(child.get())) {
            if (expr->MayHaveSideEffects()) {
                return true;
            }
        }
    }

    return false;
}

SymbolTypePtr_t AstBlockExpression::GetExprType() const
{
    AssertThrow(m_result_closure != nullptr);
    AssertThrow(m_result_closure->GetExprType() != nullptr);

    return m_result_closure->GetExprType();
}

} // namespace hyperion::compiler
