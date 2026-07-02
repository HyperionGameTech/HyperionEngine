#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstArgumentList.hpp>
#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

namespace Hyperion {

class AstVariableDeclaration;

HYP_CLASS()
class AstTemplateInstantiation : public AstTypeSpecifier
{
    HYP_OBJECT_BODY(AstTemplateInstantiation);

public:
    AstTemplateInstantiation(
        const SharedPtr<AstExpression>& expr,
        const Array<SharedPtr<AstTypeSpecifier>>& genericArgs,
        const SharedPtr<AstTypeSpecifier>& functionReturnType, // optional
        const SourceLocation& location);
    virtual ~AstTemplateInstantiation() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

private:
    Array<SharedPtr<AstTypeSpecifier>> m_genericArgs;
    SharedPtr<AstTypeSpecifier> m_functionReturnType; // optional

    SharedPtr<AstTemplateInstantiation> CloneImpl() const
    {
        return SharedPtr<AstTemplateInstantiation>(new AstTemplateInstantiation(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_genericArgs),
            CloneAstNode(m_functionReturnType),
            m_location));
    }
};

} // namespace Hyperion
