#include <script/compiler/ast/AstIsExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <core/debug/Debug.hpp>

#include <iostream>

namespace hyperion::compiler {

AstIsExpression::AstIsExpression(
    const RC<AstExpression>& target,
    const RC<AstTypeSpecifier>& typeSpecification,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD),
      m_target(target),
      m_typeSpecification(typeSpecification),
      m_isType(TRI_INDETERMINATE)
{
}

void AstIsExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_target != nullptr);
    m_target->Visit(visitor, mod);

    Assert(m_typeSpecification != nullptr);
    m_typeSpecification->Visit(visitor, mod);

    if (const auto targetType = m_target->GetExprType())
    {
        if (const auto heldType = m_typeSpecification->GetHeldType())
        {
            if (targetType->TypeCompatible(*heldType, true))
            {
                m_isType = TRI_TRUE;
            }
            else
            {
                m_isType = TRI_FALSE;
            }
        }
    }

    if (m_isType == TRI_INDETERMINATE)
    {
        // runtime check
        m_overrideExpr = visitor->GetCompilationUnit()->GetAstNodeBuilder().Module(Config::globalModuleName).Function("IsInstance").Call({ RC<AstArgument>(new AstArgument(CloneAstNode(m_target), false, false, false, false, "", m_target->GetLocation())), RC<AstArgument>(new AstArgument(CloneAstNode(m_typeSpecification->GetExpr()), false, false, false, false, "", m_typeSpecification->GetLocation())) });

        m_overrideExpr->Visit(visitor, mod);
    }
}

UniquePtr<Buildable> AstIsExpression::Build(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        return m_overrideExpr->Build(visitor, mod);
    }

    Assert(m_isType == TRI_TRUE || m_isType == TRI_FALSE);

    uint8 rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstBool>(rp, bool(m_isType.Value()));
}

void AstIsExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    if (m_overrideExpr != nullptr)
    {
        m_overrideExpr->Optimize(visitor, mod);

        return;
    }

    Assert(m_target != nullptr);
    m_target->Optimize(visitor, mod);

    Assert(m_typeSpecification != nullptr);
    m_typeSpecification->Optimize(visitor, mod);
}

RC<AstStatement> AstIsExpression::Clone() const
{
    return CloneImpl();
}

SymbolTypeRef AstIsExpression::GetExprType() const
{
    return BuiltinTypes::BOOLEAN;
}

Tribool AstIsExpression::IsTrue() const
{
    return m_isType;
}

bool AstIsExpression::MayHaveSideEffects() const
{
    Assert(
        m_target != nullptr && m_typeSpecification != nullptr);

    return m_target->MayHaveSideEffects()
        || m_typeSpecification->MayHaveSideEffects();
}

} // namespace hyperion::compiler
