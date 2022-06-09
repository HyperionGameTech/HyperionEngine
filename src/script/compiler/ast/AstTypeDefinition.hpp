#ifndef AST_TYPE_DEFINITION_HPP
#define AST_TYPE_DEFINITION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstEvent.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTypeDefinition : public AstStatement {
public:
    AstTypeDefinition(const std::string &name,
        const std::vector<std::string> &generic_params,
        const std::vector<std::shared_ptr<AstVariableDeclaration>> &members,
        const std::vector<std::shared_ptr<AstEvent>> &events,
        const SourceLocation &location);
    virtual ~AstTypeDefinition() = default;

    inline const std::string &GetName() const { return m_name; }
    inline const std::vector<std::shared_ptr<AstVariableDeclaration>>
        &GetMembers() const { return m_members; }
    inline int GetNumMembers() const { return m_num_members; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::string m_name;
    std::vector<std::string> m_generic_params;
    std::vector<std::shared_ptr<AstVariableDeclaration>> m_members;
    std::vector<std::shared_ptr<AstEvent>> m_events;
    int m_num_members;

    SymbolTypePtr_t m_event_field_type;

    inline Pointer<AstTypeDefinition> CloneImpl() const
    {
        return Pointer<AstTypeDefinition>(new AstTypeDefinition(
            m_name,
            m_generic_params,
            CloneAllAstNodes(m_members),
            CloneAllAstNodes(m_events),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
