#include <script/compiler/ast/AstYieldStatement.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/debug.h>

namespace hyperion {
namespace compiler {

AstYieldStatement::AstYieldStatement(const std::shared_ptr<AstExpression> &expr,
    const SourceLocation &location)
    : AstStatement(location),
      m_expr(expr),
      m_num_pops(0)
{
}

void AstYieldStatement::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_expr != nullptr);
    
    // transverse the scope tree to make sure we are in a function
    bool in_function = false;

    TreeNode<Scope> *top = mod->m_scopes.TopNode();
    while (top != nullptr) {
        if (top->m_value.GetScopeType() == SCOPE_TYPE_FUNCTION) {
            in_function = true;
            break;
        }
        
        m_num_pops += top->m_value.GetIdentifierTable().CountUsedVariables();
        top = top->m_parent;
    }

    if (top != nullptr && in_function) {
        m_yield_callback_call.reset(new AstCallExpression(
            std::shared_ptr<AstVariable>(new AstVariable(
                "__generator_callback",
                m_location
            )),
            { std::shared_ptr<AstArgument>(new AstArgument(
                m_expr,
                false,
                "",
                m_location
             )) },
            false,
            m_location
        ));

        m_yield_callback_call->Visit(visitor, mod);
    } else {
        // error; 'yield' not allowed outside of a function
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_yield_outside_function,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstYieldStatement::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_yield_callback_call != nullptr);
    return m_yield_callback_call->Build(visitor, mod);
}

void AstYieldStatement::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(visitor != nullptr);
    AssertThrow(mod != nullptr);

    AssertThrow(m_yield_callback_call != nullptr);

    m_yield_callback_call->Optimize(visitor, mod);
}

Pointer<AstStatement> AstYieldStatement::Clone() const
{
    return CloneImpl();
}

} // namespace compiler
} // namespace hyperion
