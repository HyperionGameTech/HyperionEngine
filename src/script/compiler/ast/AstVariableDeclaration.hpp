#ifndef AST_VARIABLE_DECLARATION_HPP
#define AST_VARIABLE_DECLARATION_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstTypeSpecification.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <memory>

namespace hyperion::compiler {

class AstVariableDeclaration : public AstDeclaration {
public:
    AstVariableDeclaration(const std::string &name,
        const std::shared_ptr<AstPrototypeSpecification> &proto,
        //const std::shared_ptr<AstTypeSpecification> &type_specification,
        const std::shared_ptr<AstExpression> &assignment,
        const std::vector<std::shared_ptr<AstParameter>> &template_params,
        bool is_const,
        bool is_generic,
        const SourceLocation &location);
    virtual ~AstVariableDeclaration() = default;

    inline const std::shared_ptr<AstExpression> &GetAssignment() const
        { return m_assignment; }
    inline const std::shared_ptr<AstExpression> &GetRealAssignment() const
        { return m_real_assignment; }

    inline bool IsConst() const { return m_is_const; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::shared_ptr<AstPrototypeSpecification> m_proto;
    //std::shared_ptr<AstTypeSpecification> m_type_specification;
    std::shared_ptr<AstExpression> m_assignment;
    std::vector<std::shared_ptr<AstParameter>> m_template_params;
    bool m_is_const;
    bool m_is_generic;

    // set while analyzing
    bool m_assignment_already_visited;
    std::shared_ptr<AstExpression> m_real_assignment;

    SymbolTypeWeakPtr_t m_symbol_type;

    inline Pointer<AstVariableDeclaration> CloneImpl() const
    {
        return Pointer<AstVariableDeclaration>(new AstVariableDeclaration(
            m_name,
            CloneAstNode(m_proto),
            //CloneAstNode(m_type_specification),
            CloneAstNode(m_assignment),
            CloneAllAstNodes(m_template_params),
            m_is_const,
            m_is_generic,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
