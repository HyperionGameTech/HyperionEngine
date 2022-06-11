#ifndef AST_ENUM_EXPRESSION_HPP
#define AST_ENUM_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

struct EnumEntry {
    std::string name;
    std::shared_ptr<AstExpression> assignment;
    SourceLocation location;
};

class AstEnumExpression : public AstExpression {
public:
    AstEnumExpression(
        const std::string &name,
        const std::vector<EnumEntry> &entries,
        const SourceLocation &location);
    virtual ~AstEnumExpression() = default;

    inline const std::string &GetName() const { return m_name; }
    inline void SetName(const std::string &name) { m_name = name; }

    inline const std::vector<EnumEntry> &GetEntries() const { return m_entries; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual bool IsLiteral() const override;
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const AstExpression *GetValueOf() const override;

protected:
    std::string m_name;
    std::vector<EnumEntry> m_entries;

    SymbolTypePtr_t m_symbol_type;

    std::shared_ptr<AstTypeObject> m_expr;

    inline Pointer<AstEnumExpression> CloneImpl() const
    {
        return Pointer<AstEnumExpression>(new AstEnumExpression(
            m_name,
            m_entries,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
