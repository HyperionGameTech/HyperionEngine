#ifndef AST_PARAMETER_HPP
#define AST_PARAMETER_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <core/lib/String.hpp>

namespace hyperion::compiler {

class AstParameter : public AstDeclaration
{
public:
    AstParameter(
        const String &name,
        const RC<AstPrototypeSpecification> &type_spec,
        const RC<AstExpression> &default_param,
        Bool is_variadic,
        Bool is_const,
        Bool is_ref,
        const SourceLocation &location
    );

    virtual ~AstParameter() override = default;

    const RC<AstExpression> &GetDefaultValue() const
        { return m_default_param; }

    void SetDefaultValue(const RC<AstExpression> &default_param)
        { m_default_param = default_param; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    Bool IsVariadic() const { return m_is_variadic; }
    Bool IsConst() const { return m_is_const; }
    Bool IsRef() const { return m_is_ref; }

    Bool IsGenericParam() const { return m_is_generic_param; }
    void SetIsGenericParam(bool is_generic_param) { m_is_generic_param = is_generic_param; }

    // used by AstTemplateExpression
    const RC<AstPrototypeSpecification> &GetPrototypeSpecification() const
        { return m_type_spec; } 

    void SetPrototypeSpecification(const RC<AstPrototypeSpecification> &type_spec)
        { m_type_spec = type_spec; }

    SymbolTypePtr_t GetExprType() const;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstDeclaration::GetHashCode().Add(TypeName<AstParameter>());
        hc.Add(m_type_spec ? m_type_spec->GetHashCode() : HashCode());
        hc.Add(m_default_param ? m_default_param->GetHashCode() : HashCode());
        hc.Add(m_is_variadic);
        hc.Add(m_is_const);
        hc.Add(m_is_ref);
        hc.Add(m_is_generic_param);

        return hc;
    }

private:
    RC<AstPrototypeSpecification>   m_type_spec;
    RC<AstExpression>               m_default_param;
    Bool                            m_is_variadic;
    Bool                            m_is_const;
    Bool                            m_is_ref;
    Bool                            m_is_generic_param;

    // Set while analyzing
    SymbolTypePtr_t                 m_symbol_type;
    RC<AstExpression>               m_varargs_type_spec;

    RC<AstParameter> CloneImpl() const
    {
        return RC<AstParameter>(new AstParameter(
            m_name,
            CloneAstNode(m_type_spec),
            CloneAstNode(m_default_param),
            m_is_variadic,
            m_is_const,
            m_is_ref,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
