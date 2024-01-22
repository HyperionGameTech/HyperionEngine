#ifndef AST_TYPE_OBJECT_HPP
#define AST_TYPE_OBJECT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariable.hpp>

namespace hyperion::compiler {

class AstTypeRef;

class AstTypeObject : public AstExpression
{
public:
    AstTypeObject(
        const SymbolTypePtr_t &symbol_type,
        const SymbolTypePtr_t &base_symbol_type, // base here is usually CLASS_TYPE - it is not the same as polymorphic base
        const SourceLocation &location
    );

    AstTypeObject(
        const SymbolTypePtr_t &symbol_type,
        const SymbolTypePtr_t &base_symbol_type,
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

    bool IsVisited() const
        { return m_is_visited; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTypeObject>());
        hc.Add(m_symbol_type ? m_symbol_type->GetHashCode() : HashCode());
        hc.Add(m_base_symbol_type ? m_base_symbol_type->GetHashCode() : HashCode());
        hc.Add(m_enum_underlying_type ? m_enum_underlying_type->GetHashCode() : HashCode());
        hc.Add(m_is_proxy_class);

        return hc;
    }

private:
    SymbolTypePtr_t             m_symbol_type;
    SymbolTypePtr_t             m_base_symbol_type;
    SymbolTypePtr_t             m_enum_underlying_type;
    bool                        m_is_proxy_class;

    // set while analyzing
    RC<AstTypeRef>              m_base_type_ref;
    Array<RC<AstExpression>>    m_member_expressions;
    bool                        m_is_visited;

    RC<AstTypeObject> CloneImpl() const
    {
        return RC<AstTypeObject>(new AstTypeObject(
            m_symbol_type,
            m_base_symbol_type,
            m_enum_underlying_type,
            m_is_proxy_class,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
