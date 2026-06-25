#include <Lang/Compiler/Ast/AstReturnStatement.hpp>
#include <Lang/Compiler/Ast/AstAsExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstTypeRef.hpp>
#include <Lang/Compiler/Optimizer.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Compiler.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Keywords.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Lang/Instructions.hpp>
#include <Core/Debug/Debug.hpp>

#include <AstReturnStatement.generated.inl>

namespace Hyperion {

AstReturnStatement::AstReturnStatement(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstStatement(location),
      m_expr(expr),
      m_exprType(nullptr),
      m_numPops(0),
      m_isConstructor(false)
{
}

void AstReturnStatement::Visit(AstVisitor* visitor, Module* mod)
{
    m_exprType = m_expr ? BuiltinTypes::s_errorType : BuiltinTypes::s_voidType;

    if (m_expr != nullptr)
    {
        m_expr->Visit(visitor, mod);

        m_exprType = m_expr->GetExprType();
        Assert(m_exprType != nullptr);

        // Find the ReturnType placeholder type:
        const SymbolType* returnType = mod->LookupSymbolType("ReturnType", /* includePlaceholderTypes */ true);

        // should be defined as long as return is used in a valid function context.
        // it's an error otherwise, but could still happen so we need to do a null check.
        if (!returnType)
        {
            visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_return_outside_function,
                m_location));

            return;
        }

        // if returnType is a placeholder type, we don't want to do any type compatibility checks
        // but otherwise, we need to make sure the returned expression is compatible with the function's return type.
        // if it's not equal, we'll need to do a cast.
        if (!returnType->IsPlaceholderType())
        {
            if (!returnType->IsAnyType() && !returnType->TypeEqual(*m_exprType))
            {
                if (returnType->TypeCompatible(
                        *m_exprType,
                        /* strictNumbers */ false,
                        /* strictAny */ false,
                        /* strictEnum */ false,
                        /* strictNull */ true,
                        /* strictDownCasting */ true))
                {
                    // insert cast
                    m_overrideExpr.Reset(new AstAsExpression(
                        CloneAstNode(m_expr),
                        RC<AstTypeSpecifier>(new AstTypeSpecifier(
                            RC<AstTypeRef>(new AstTypeRef(returnType, m_location)),
                            m_location)),
                        m_location));

                    m_overrideExpr->Visit(visitor, mod);

                    m_exprType = m_overrideExpr->GetExprType();
                    Assert(m_exprType != nullptr);
                }
            }
        }
    }

    // transverse the scope tree to make sure we are in a function
    bool inFunction = false;

    TreeNode<Scope>* top = mod->scopeTree.TopNode();

    while (top != nullptr)
    {
        if (top->Get().scopeType == SCOPE_TYPE_FUNCTION)
        {
            inFunction = true;

            if (top->Get().scopeFlags & CONSTRUCTOR_DEFINITION_FLAG)
            {
                m_isConstructor = true;
            }

            break;
        }

        m_numPops += top->Get().identifierTable.CountUsedVariables();
        top = top->m_parent;
    }

    if (inFunction)
    {
        Assert(top != nullptr);

        top->Get().returnTypes.PushBack(m_exprType);

        return;
    }

    // error; 'return' not allowed outside of a function
    visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_return_outside_function,
        m_location));
}

UniquePtr<Buildable> AstReturnStatement::Build(AstVisitor* visitor, Module* mod)
{
    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    const RC<AstExpression>& expr = m_overrideExpr ? m_overrideExpr : m_expr;

    if (expr != nullptr)
    {
        chunk->Append(expr->Build(visitor, mod));

        chunk->Append(Compiler::DerefIfNeeded(visitor, mod, m_exprType));
    }

    chunk->Append(Compiler::PopStack(visitor, m_numPops));

    // add RET instruction
    auto instrRet = BytecodeUtil::Make<RawOperation<>>();
    instrRet->opcode = RET;
    chunk->Append(std::move(instrRet));

    return chunk;
}

void AstReturnStatement::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);
    }
    else if (m_expr != nullptr)
    {
        m_expr->Optimize(visitor, mod);
    }
}

RC<AstStatement> AstReturnStatement::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
