#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstHasExpression.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Compiler.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Lang/Compiler/Emit/BytecodeChunk.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Unicode.hpp>

#include <AstTypeSpecifier.generated.inl>

namespace Hyperion {

AstTypeSpecifier::AstTypeSpecifier(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr),
      m_symbolType(nullptr)
{
}

void AstTypeSpecifier::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(visitor != nullptr);
    Assert(mod != nullptr);

    // open scope to set type spec context (so variable names will be looked up as types only)
    // this prevents issues such as using MyClass as a param type in a function inside MyClass
    // being interpreted as the constructor function, not the type MyClass.
    ScopeGuard scope(mod, SCOPE_TYPE_TYPE_SPECIFICATION);

    Assert(m_expr != nullptr);
    m_expr->Visit(visitor, mod);

    m_symbolType = BuiltinTypes::s_errorType;

    const SymbolType* heldType = m_expr->GetHeldType();

    if (!heldType)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_invalid_type_specifier,
            m_location));

        return;
    }

    m_symbolType = heldType->GetUnaliased();

    Assert(m_symbolType != nullptr && m_symbolType->IsRegistered());
}

UniquePtr<Buildable> AstTypeSpecifier::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    return m_expr->Build(visitor, mod);
}

void AstTypeSpecifier::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_expr != nullptr);
    m_expr->Optimize(visitor, mod);
}

RC<AstStatement> AstTypeSpecifier::Clone() const
{
    return CloneImpl();
}

Tribool AstTypeSpecifier::IsTrue() const
{
    return Tribool::True();
}

bool AstTypeSpecifier::MayHaveSideEffects() const
{
    Assert(m_expr != nullptr);
    return m_expr->MayHaveSideEffects();
}

const SymbolType* AstTypeSpecifier::GetExprType() const
{
    if (m_expr != nullptr)
    {
        return m_expr->GetExprType();
    }

    return nullptr;
}

const SymbolType* AstTypeSpecifier::GetHeldType() const
{
    return m_symbolType;
}

const AstExpression* AstTypeSpecifier::GetValueOf() const
{
    if (m_expr != nullptr)
    {
        return m_expr->GetValueOf();
    }

    return AstExpression::GetValueOf();
}

const AstExpression* AstTypeSpecifier::GetDeepValueOf() const
{
    if (m_expr != nullptr)
    {
        return m_expr->GetDeepValueOf();
    }

    return AstExpression::GetDeepValueOf();
}

} // namespace Hyperion
