#ifndef AST_TYPE_OBJECT_HPP
#define AST_TYPE_OBJECT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>

namespace hyperion::compiler {

class AstTypeObject : public AstExpression
{
public:
    AstTypeObject(
        const SymbolTypePtr_t &symbol_type,
        const RC<AstVariable> &proto,
        const SourceLocation &location
    );

    AstTypeObject(
        const SymbolTypePtr_t &symbol_type,
        const RC<AstVariable> &proto,
        const SymbolTypePtr_t &enum_underlying_type,
        bool is_proxy_class,
        const SourceLocation &location
    );

    virtual ~AstTypeObject() = default;

    bool IsEnum() const
        { return m_enum_underlying_type != nullptr; }

    const SymbolTypePtr_t &GetEnumUnderlyingType() const
        { return m_enum_underlying_type; }

    bool IsProxyClass() const
        { return m_is_proxy_class; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    const SymbolTypePtr_t &GetHeldType() const
        { return m_symbol_type; }

private:
    SymbolTypePtr_t m_symbol_type;
    RC<AstVariable> m_proto;
    SymbolTypePtr_t m_enum_underlying_type;
    bool m_is_proxy_class;

    // set while analyzing
    Array<RC<AstExpression>> m_member_expressions;

    RC<AstTypeObject> CloneImpl() const
    {
        return RC<AstTypeObject>(new AstTypeObject(
            m_symbol_type,
            CloneAstNode(m_proto),
            m_enum_underlying_type,
            m_is_proxy_class,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
