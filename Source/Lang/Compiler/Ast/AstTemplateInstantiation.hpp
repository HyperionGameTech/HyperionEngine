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
        const Handle<AstExpression>& expr,
        const Array<Handle<AstTypeSpecifier>>& genericArgs,
        const Handle<AstTypeSpecifier>& functionReturnType, // optional
        const SourceLocation& location);
    virtual ~AstTemplateInstantiation() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

private:
    Array<Handle<AstTypeSpecifier>> m_genericArgs;
    Handle<AstTypeSpecifier> m_functionReturnType; // optional

    Handle<AstTemplateInstantiation> CloneImpl() const
    {
        return MakeHandle<AstTemplateInstantiation>(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_genericArgs),
            CloneAstNode(m_functionReturnType),
            m_location);
    }
};

} // namespace Hyperion
