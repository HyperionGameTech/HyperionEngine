#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/Optimizer.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>

namespace hyperion::compiler {

AstReturnStatement::AstReturnStatement(const std::shared_ptr<AstExpression> &expr,
    const SourceLocation &location)
    : AstStatement(location),
      m_expr(expr),
      m_num_pops(0)
{
}

void AstReturnStatement::Visit(AstVisitor *visitor, Module *mod)
{
    if (m_expr != nullptr) {
        m_expr->Visit(visitor, mod);
    }

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

    if (in_function) {
        AssertThrow(top != nullptr);
        // add return type

        if (m_expr != nullptr) {
            top->m_value.AddReturnType(m_expr->GetExprType(), m_location);
        } else {
            top->m_value.AddReturnType(BuiltinTypes::VOID_TYPE, m_location);
        }
    } else {
        // error; 'return' not allowed outside of a function
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_return_outside_function,
            m_location
        ));
    }
}

std::unique_ptr<Buildable> AstReturnStatement::Build(AstVisitor *visitor, Module *mod)
{
    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    if (m_expr != nullptr) {
        chunk->Append(m_expr->Build(visitor, mod));
    }

    chunk->Append(Compiler::PopStack(visitor, m_num_pops));

    // add RET instruction
    auto instr_ret = BytecodeUtil::Make<RawOperation<>>();
    instr_ret->opcode = RET;
    chunk->Append(std::move(instr_ret));
    
    return std::move(chunk);
}

void AstReturnStatement::Optimize(AstVisitor *visitor, Module *mod)
{
    if (m_expr != nullptr) {
        m_expr->Optimize(visitor, mod);
    }
}

Pointer<AstStatement> AstReturnStatement::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
