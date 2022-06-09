#ifndef AST_PROTOTYPE_DEFINITION_HPP
#define AST_PROTOTYPE_DEFINITION_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstTypeSpecification.hpp>
#include <script/compiler/ast/AstEvent.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstPrototypeDefinition : public AstDeclaration {
public:
    AstPrototypeDefinition(const std::string &name,
        const std::shared_ptr<AstTypeSpecification> &base_specification,
        const std::vector<std::string> &generic_params,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &static_members,
        const std::vector<std::shared_ptr<AstEvent>> &events,
        const SourceLocation &location);
    virtual ~AstPrototypeDefinition() = default;

    inline const std::string &GetName() const { return m_name; }
    inline const std::vector<std::shared_ptr<AstVariableDeclaration>>
        &GetMembers() const { return m_members; }
    inline int GetNumMembers() const { return m_num_members; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::shared_ptr<AstTypeSpecification> m_base_specification;
    std::vector<std::string> m_generic_params;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_members;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_static_members;
    std::vector<std::shared_ptr<AstEvent>> m_events;
    int m_num_members;

    SymbolTypePtr_t m_event_field_type;

    SymbolTypePtr_t m_symbol_type;

    inline Pointer<AstPrototypeDefinition> CloneImpl() const
    {
        return Pointer<AstPrototypeDefinition>(new AstPrototypeDefinition(
            m_name,
            CloneAstNode(m_base_specification),
            m_generic_params,
            CloneAllAstNodes(m_members),
            CloneAllAstNodes(m_static_members),
            CloneAllAstNodes(m_events),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
