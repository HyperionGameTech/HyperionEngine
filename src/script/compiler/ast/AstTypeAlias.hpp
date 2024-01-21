#ifndef AST_TYPE_ALIAS_HPP
#define AST_TYPE_ALIAS_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <core/lib/String.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

class AstTypeAlias: public AstStatement
{
public:
    AstTypeAlias(
        const String &name,
        const RC<AstPrototypeSpecification> &aliasee,
        const SourceLocation &location
    );
    virtual ~AstTypeAlias() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstTypeAlias>());
        hc.Add(m_name);
        hc.Add(m_aliasee ? m_aliasee->GetHashCode() : HashCode());

        return hc;
    }

private:
    String                          m_name;
    RC<AstPrototypeSpecification>   m_aliasee;

    RC<AstTypeAlias> CloneImpl() const
    {
        return RC<AstTypeAlias>(new AstTypeAlias(
            m_name,
            CloneAstNode(m_aliasee),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
