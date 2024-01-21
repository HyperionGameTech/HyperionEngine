#ifndef AST_ENUM_EXPRESSION_HPP
#define AST_ENUM_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>
#include <core/lib/String.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

struct EnumEntry
{
    String              name;
    RC<AstExpression>   assignment;
    SourceLocation      location;

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(assignment ? assignment->GetHashCode() : HashCode());

        return hc;
    }
};

class AstEnumExpression : public AstExpression
{
public:
    AstEnumExpression(
        const String &name,
        const Array<EnumEntry> &entries,
        const RC<AstPrototypeSpecification> &underlying_type,
        const SourceLocation &location
    );
    virtual ~AstEnumExpression() = default;

    void SetName(const String &name) { m_name = name; }

    const Array<EnumEntry> &GetEntries() const { return m_entries; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual bool IsLiteral() const override;
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual const String &GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstEnumExpression>());
        hc.Add(m_name);

        for (auto &entry : m_entries) {
            hc.Add(entry.GetHashCode());
        }

        hc.Add(m_underlying_type ? m_underlying_type->GetHashCode() : HashCode());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

protected:
    String                          m_name;
    Array<EnumEntry>                m_entries;
    RC<AstPrototypeSpecification>   m_underlying_type;
    RC<AstTypeExpression>           m_expr;

    RC<AstEnumExpression> CloneImpl() const
    {
        return RC<AstEnumExpression>(new AstEnumExpression(
            m_name,
            m_entries,
            CloneAstNode(m_underlying_type),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
