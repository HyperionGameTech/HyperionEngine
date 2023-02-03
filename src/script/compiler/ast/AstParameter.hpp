#ifndef AST_PARAMETER_HPP
#define AST_PARAMETER_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion::compiler {

class AstParameter : public AstDeclaration {
public:
    AstParameter(
        const std::string &name,
        const RC<AstPrototypeSpecification> &type_spec,
        const RC<AstExpression> &default_param,
        bool is_variadic,
        bool is_const,
        bool is_ref,
        const SourceLocation &location
    );

    virtual ~AstParameter() = default;

    const RC<AstExpression> &GetDefaultValue() const
        { return m_default_param; }
    void SetDefaultValue(const RC<AstExpression> &default_param)
        { m_default_param = default_param; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    bool IsVariadic() const { return m_is_variadic; }
    bool IsConst() const { return m_is_const; }
    bool IsRef() const { return m_is_ref; }

    bool IsGenericParam() const { return m_is_generic_param; }
    void SetIsGenericParam(bool is_generic_param) { m_is_generic_param = is_generic_param; }

    // used by AstTemplateExpression
    const RC<AstPrototypeSpecification> &GetPrototypeSpecification() const
        { return m_type_spec; } 

    void SetPrototypeSpecification(const RC<AstPrototypeSpecification> &type_spec)
        { m_type_spec = type_spec; }

private:
    RC<AstPrototypeSpecification> m_type_spec;
    RC<AstExpression> m_default_param;
    bool m_is_variadic;
    bool m_is_const;
    bool m_is_ref;
    bool m_is_generic_param;

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
