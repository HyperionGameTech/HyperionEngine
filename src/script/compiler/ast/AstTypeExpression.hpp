#ifndef AST_TYPE_EXPRESSION_HPP
#define AST_TYPE_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTypeExpression : public AstExpression
{
public:
    AstTypeExpression(
        const String &name,
        const RC<AstPrototypeSpecification> &base_specification,
        const Array<RC<AstVariableDeclaration>> &data_members,
        const Array<RC<AstVariableDeclaration>> &function_members,
        const Array<RC<AstVariableDeclaration>> &static_members,
        bool is_proxy_class,
        const SourceLocation &location
    );

    AstTypeExpression(
        const String &name,
        const RC<AstPrototypeSpecification> &base_specification,
        const Array<RC<AstVariableDeclaration>> &data_members,
        const Array<RC<AstVariableDeclaration>> &function_members,
        const Array<RC<AstVariableDeclaration>> &static_members,
        const SymbolTypePtr_t &enum_underlying_type,
        bool is_proxy_class,
        const SourceLocation &location
    );

    virtual ~AstTypeExpression() override = default;

    /** enable setting to that variable declarations can change the type name */
    void SetName(const String &name)
        { m_name = name; }

    Array<RC<AstVariableDeclaration>> &GetDataMembers()
        { return m_data_members; }

    const Array<RC<AstVariableDeclaration>> &GetDataMembers() const
        { return m_data_members; }

    Array<RC<AstVariableDeclaration>> &GetFunctionMembers()
        { return m_function_members; }

    const Array<RC<AstVariableDeclaration>> &GetFunctionMembers() const
        { return m_function_members; }

    Array<RC<AstVariableDeclaration>> &GetStaticMembers()
        { return m_static_members; }

    const Array<RC<AstVariableDeclaration>> &GetStaticMembers() const
        { return m_function_members; }

    bool IsEnum() const
        { return m_enum_underlying_type != nullptr; }

    bool IsProxyClass() const
        { return m_is_proxy_class; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;

    virtual bool IsLiteral() const override;
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;

    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

    virtual const String &GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTypeExpression>());
        hc.Add(m_name);
        hc.Add(m_base_specification ? m_base_specification->GetHashCode() : HashCode());

        for (auto &member : m_data_members) {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto &member : m_function_members) {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto &member : m_static_members) {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        if (m_enum_underlying_type != nullptr) {
            hc.Add(m_enum_underlying_type->GetHashCode());
        }

        hc.Add(m_is_proxy_class);

        return hc;
    }

protected:
    String                              m_name;
    RC<AstPrototypeSpecification>       m_base_specification;
    Array<RC<AstVariableDeclaration>>   m_data_members;
    Array<RC<AstVariableDeclaration>>   m_function_members;
    Array<RC<AstVariableDeclaration>>   m_static_members;
    SymbolTypePtr_t                     m_enum_underlying_type;
    bool                                m_is_proxy_class;

    SymbolTypePtr_t                     m_symbol_type;

    RC<AstTypeObject>                   m_type_object;
    RC<AstTypeObject>                   m_prototype_expr;
    RC<AstTypeRef>                      m_type_ref;
    Array<RC<AstVariableDeclaration>>   m_outside_members;
    Array<RC<AstVariableDeclaration>>   m_combined_members;
    bool                                m_is_uninstantiated_generic;
    bool                                m_is_visited;

    RC<AstTypeExpression> CloneImpl() const
    {
        return RC<AstTypeExpression>(new AstTypeExpression(
            m_name,
            CloneAstNode(m_base_specification),
            CloneAllAstNodes(m_data_members),
            CloneAllAstNodes(m_function_members),
            CloneAllAstNodes(m_static_members),
            m_enum_underlying_type,
            m_is_proxy_class,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
