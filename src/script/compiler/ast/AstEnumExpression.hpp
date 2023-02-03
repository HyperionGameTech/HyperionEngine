#ifndef AST_ENUM_EXPRESSION_HPP
#define AST_ENUM_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

struct EnumEntry
{
    std::string name;
    RC<AstExpression> assignment;
    SourceLocation location;
};

class AstEnumExpression : public AstExpression
{
public:
    AstEnumExpression(
        const std::string &name,
        const std::vector<EnumEntry> &entries,
        const RC<AstPrototypeSpecification> &underlying_type,
        const SourceLocation &location
    );
    virtual ~AstEnumExpression() = default;

    void SetName(const std::string &name) { m_name = name; }

    const std::vector<EnumEntry> &GetEntries() const { return m_entries; }

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
    virtual const std::string &GetName() const override;

protected:
    std::string                                m_name;
    std::vector<EnumEntry>                     m_entries;
    RC<AstPrototypeSpecification> m_underlying_type;

    RC<AstTypeExpression>         m_expr;

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
