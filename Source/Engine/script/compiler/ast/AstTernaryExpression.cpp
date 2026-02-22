#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

#include <cstdio>

namespace Hyperion {

AstTernaryExpression::AstTernaryExpression(
    const RC<AstExpression>& conditional,
    const RC<AstExpression>& left,
    const RC<AstExpression>& right,
    const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ((right && left) ? (left->GetAccessOptions() & right->GetAccessOptions()) : 0)),
      m_conditional(conditional),
      m_left(left),
      m_right(right)
{
}

void AstTernaryExpression::Visit(AstVisitor* visitor, Module* mod)
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    m_conditional->Visit(visitor, mod);

    m_left->SetAccessMode(GetAccessMode());
    m_right->SetAccessMode(GetAccessMode());

    if (m_conditional->IsTrue() != TRI_FALSE)
    {
        m_left->Visit(visitor, mod);
    }

    if (m_conditional->IsTrue() != TRI_TRUE)
    {
        m_right->Visit(visitor, mod);
    }

    const SymbolType* exprType = GetExprType();
    Assert(exprType != nullptr);

    if (exprType->TypeEqual(*BuiltinTypes::s_errorType))
    {
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_mismatched_types,
            m_location,
            m_left->GetExprType()->ToString(),
            m_right->GetExprType()->ToString()));
    }
}

UniquePtr<Buildable> AstTernaryExpression::Build(AstVisitor* visitor, Module* mod)
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    UniquePtr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    int conditionIsTrue = m_conditional->IsTrue();

    if (conditionIsTrue == -1)
    {
        // the condition cannot be determined at compile time
        chunk->Append(Compiler::CreateConditional(
            visitor,
            mod,
            m_conditional.Get(),
            m_left.Get(),
            m_right.Get()));
    }
    else if (conditionIsTrue)
    {
        // the condition has been determined to be true
        if (m_conditional->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }
        // enter the block
        chunk->Append(m_left->Build(visitor, mod));
        // do not accept the else-block
    }
    else
    {
        // the condition has been determined to be false
        if (m_conditional->MayHaveSideEffects())
        {
            // if there is a possibility of side effects,
            // build the conditional into the binary
            chunk->Append(m_conditional->Build(visitor, mod));
        }

        chunk->Append(m_right->Build(visitor, mod));
    }

    return chunk;
}

void AstTernaryExpression::Optimize(AstVisitor* visitor, Module* mod)
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    m_conditional->Optimize(visitor, mod);
    m_left->Optimize(visitor, mod);
    m_right->Optimize(visitor, mod);
}

RC<AstStatement> AstTernaryExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstTernaryExpression::IsTrue() const
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    switch (int(m_conditional->IsTrue()))
    {
    case TRI_INDETERMINATE:
        return Tribool::Indeterminate();
    case TRI_FALSE:
        return m_right->IsTrue();
    case TRI_TRUE:
        return m_left->IsTrue();
    }

    return Tribool::Indeterminate();
}

bool AstTernaryExpression::MayHaveSideEffects() const
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    if (m_conditional->MayHaveSideEffects())
    {
        return true;
    }

    switch (int(m_conditional->IsTrue()))
    {
    case TRI_INDETERMINATE:
        return m_left->MayHaveSideEffects() || m_right->MayHaveSideEffects();
    case TRI_TRUE:
        return m_left->MayHaveSideEffects();
    case TRI_FALSE:
        return m_right->MayHaveSideEffects();
    }

    return false;
}

const SymbolType* AstTernaryExpression::GetExprType() const
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    switch (int(m_conditional->IsTrue()))
    {
    case TRI_INDETERMINATE:
    {
        Assert(m_left != nullptr);

        const SymbolType* lTypePtr = m_left->GetExprType();
        Assert(lTypePtr != nullptr);

        if (m_right != nullptr)
        {
            // the right was not optimized away,
            // return type promotion
            const SymbolType* rTypePtr = m_right->GetExprType();
            Assert(rTypePtr != nullptr);

            return SymbolType::TypePromotion(lTypePtr, rTypePtr);
        }
        else
        {
            // right was optimized away, return only left type
            return lTypePtr;
        }
    }
    case TRI_TRUE:
        return m_left->GetExprType();
    case TRI_FALSE:
        return m_right->GetExprType();
    }

    return nullptr;
}

bool AstTernaryExpression::IsLiteral() const
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    const auto conditionalValue = m_conditional->IsTrue();

    if (conditionalValue == Tribool::Indeterminate())
    {
        return false;
    }

    if (conditionalValue == Tribool::True())
    {
        return m_left->IsLiteral();
    }

    return m_right->IsLiteral();
}

bool AstTernaryExpression::IsMutable() const
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    return m_left->IsMutable() && m_right->IsMutable();
}

const AstExpression* AstTernaryExpression::GetValueOf() const
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    const auto conditionalValue = m_conditional->IsTrue();

    if (conditionalValue == Tribool::Indeterminate())
    {
        return AstExpression::GetValueOf();
    }

    if (conditionalValue == Tribool::True())
    {
        return m_left->GetValueOf();
    }

    return m_right->GetValueOf();
}

const AstExpression* AstTernaryExpression::GetDeepValueOf() const
{
    Assert(m_conditional != nullptr);
    Assert(m_left != nullptr);
    Assert(m_right != nullptr);

    const auto conditionalValue = m_conditional->IsTrue();

    if (conditionalValue == Tribool::Indeterminate())
    {
        return AstExpression::GetDeepValueOf();
    }

    if (conditionalValue == Tribool::True())
    {
        return m_left->GetDeepValueOf();
    }

    return m_right->GetDeepValueOf();
}

} // namespace Hyperion
