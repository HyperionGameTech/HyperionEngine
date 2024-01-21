#ifndef AST_MODULE_ACCESS_HPP
#define AST_MODULE_ACCESS_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <core/lib/String.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstModuleAccess : public AstExpression
{
public:
    AstModuleAccess(
        const String &target,
        const RC<AstExpression> &expr,
        const SourceLocation &location
    );
    virtual ~AstModuleAccess() override = default;

    Module *GetModule()
        { return m_mod_access; }

    const Module *GetModule() const
        { return m_mod_access; }

    const String &GetTargetName() const
        { return m_target; }

    const RC<AstExpression> &GetExpression() const
        { return m_expr; }

    void SetExpression(const RC<AstExpression> &expr)
        { m_expr = expr; }

    void SetChained(Bool is_chained)
        { m_is_chained = is_chained; }

    void PerformLookup(AstVisitor *visitor, Module *mod);

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual AstExpression *GetTarget() const override;
    virtual bool IsMutable() const override;
    virtual bool IsLiteral() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstModuleAccess>());
        hc.Add(m_target);
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

private:
    String              m_target;
    RC<AstExpression>   m_expr;

    // set while analyzing
    Module              *m_mod_access;
    // is this module access chained to another before it?
    bool                m_is_chained;
    bool                m_looked_up;

    RC<AstModuleAccess> CloneImpl() const
    {
        return RC<AstModuleAccess>(new AstModuleAccess(
            m_target,
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
