#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/containers/String.hpp>

#include <core/HashCode.hpp>

namespace hyperion::compiler {

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

    bool IsSplat() const
    {
        return m_isSplat;
    }

    bool IsNamed() const
    {
        return m_isNamed;
    }

    bool IsPassConst() const
    {
        return m_isPassConst;
    }

    void SetIsPassConst(bool isPassConst)
    {
        m_isPassConst = isPassConst;
    }

    bool IsPassByRef() const
    {
        return m_isPassByRef;
    }

    void SetIsPassByRef(bool isPassByRef)
    {
        m_isPassByRef = isPassByRef;
    }

    bool IsPlaceholderArgument() const
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
    virtual SymbolTypeRef GetExprType() const override;
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
    bool m_isSplat;
    bool m_isNamed;
    bool m_isPassByRef;
    bool m_isPassConst;
    String m_name;

    bool m_isVisited = false;

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

} // namespace hyperion::compiler
