#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <Core/containers/String.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

class AstArgument : public AstExpression
{
public:
    AstArgument(
        const RC<AstExpression>& expr,
        bool isSplat,
        bool isNamed,
        bool isPassByRef,
        bool isPassConst,
        const String& name,
        const SourceLocation& location);
    virtual ~AstArgument() override = default;

    const RC<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    HYP_FORCE_INLINE bool IsSplat() const
    {
        return m_isSplat;
    }

    HYP_FORCE_INLINE bool IsNamed() const
    {
        return m_isNamed;
    }

    HYP_FORCE_INLINE bool IsPassConst() const
    {
        return m_isPassConst;
    }

    HYP_FORCE_INLINE void SetIsPassConst(bool isPassConst)
    {
        m_isPassConst = isPassConst;
    }

    HYP_FORCE_INLINE bool IsPassByRef() const
    {
        return m_isPassByRef;
    }

    HYP_FORCE_INLINE void SetIsPassByRef(bool isPassByRef)
    {
        m_isPassByRef = isPassByRef;
    }

    HYP_FORCE_INLINE bool IsPlaceholderArgument() const
    {
        return m_expr == nullptr;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    // virtual const AstExpression *GetValueOf() const override { return m_expr.Get(); }

    virtual bool IsLiteral() const override;
    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;
    virtual const String& GetName() const override;

    virtual String ToString() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArgument>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());
        hc.Add(m_isSplat);
        hc.Add(m_isNamed);
        hc.Add(m_isPassByRef);
        hc.Add(m_isPassConst);
        hc.Add(m_name);

        return hc;
    }

private:
    RC<AstExpression> m_expr;
    bool m_isSplat : 1;
    bool m_isNamed : 1;
    bool m_isPassByRef : 1;
    bool m_isPassConst : 1;
    String m_name;

    RC<AstArgument> CloneImpl() const
    {
        return RC<AstArgument>(new AstArgument(
            CloneAstNode(m_expr),
            m_isSplat,
            m_isNamed,
            m_isPassByRef,
            m_isPassConst,
            m_name,
            m_location));
    }
};

} // namespace Hyperion
