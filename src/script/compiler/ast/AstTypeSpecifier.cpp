#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Compiler.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <core/debug/Debug.hpp>
#include <util/UTF8.hpp>

namespace hyperion {

AstTypeSpecifier::AstTypeSpecifier(
    const RC<AstExpression>& expr,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_expr(expr)
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

    SymbolTypeRef heldType = m_expr->GetHeldType();

    if (!heldType)
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_type,
            m_location,
            m_expr->GetExprType()->ToString()));

        return;
    }

    heldType = heldType->GetUnaliased();

    if (heldType->IsEnumType())
    {
        Assert(heldType->GetGenericInstanceInfo().m_genericArgs.Size() == 1);

        SymbolTypeRef enumUnderlyingType = heldType->GetGenericInstanceInfo().m_genericArgs.Front().m_type;
        Assert(enumUnderlyingType != nullptr);
        enumUnderlyingType = enumUnderlyingType->GetUnaliased();

        // for enum types, we use the underlying type.
        heldType = enumUnderlyingType;
    }

    m_symbolType = heldType;

    Assert(m_symbolType != nullptr);
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

SymbolTypeRef AstTypeSpecifier::GetExprType() const
{
    if (m_expr != nullptr)
    {
        return m_expr->GetExprType();
    }

    return nullptr;
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

SymbolTypeRef AstTypeSpecifier::GetHeldType() const
{
    return m_symbolType;
}

} // namespace hyperion
