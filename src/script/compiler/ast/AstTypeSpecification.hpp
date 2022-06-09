#ifndef AST_TYPE_SPECIFICATION_HPP
#define AST_TYPE_SPECIFICATION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

class AstTypeSpecification: public AstStatement {
public:
    AstTypeSpecification(
        const std::string &left,
        const std::vector<std::shared_ptr<AstTypeSpecification>> &generic_params,
        const std::shared_ptr<AstTypeSpecification> &right,
        const SourceLocation &location);
    virtual ~AstTypeSpecification() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    inline const SymbolTypePtr_t &GetSpecifiedType() const { return m_symbol_type; }
    inline const SymbolTypePtr_t &GetOriginalType() const { return m_original_type; }

private:
    std::string m_left;
    std::vector<std::shared_ptr<AstTypeSpecification>> m_generic_params;
    std::shared_ptr<AstTypeSpecification> m_right;

    /** Set while analyzing */
    SymbolTypePtr_t m_symbol_type;
    SymbolTypePtr_t m_original_type;

    /** Is module access chained */
    bool m_is_chained;

    inline Pointer<AstTypeSpecification> CloneImpl() const
    {
        return Pointer<AstTypeSpecification>(new AstTypeSpecification(
            m_left,
            CloneAllAstNodes(m_generic_params),
            CloneAstNode(m_right),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
