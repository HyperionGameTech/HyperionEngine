#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

namespace Hyperion {

class AstVariableDeclaration;

class AstTemplateInstantiation : public AstTypeSpecifier
{
public:
    AstTemplateInstantiation(
        const RC<AstExpression>& expr,
        const Array<RC<AstTypeSpecifier>>& genericArgs,
        const RC<AstTypeSpecifier>& functionReturnType, // optional
        const SourceLocation& location);
    virtual ~AstTemplateInstantiation() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

private:
    Array<RC<AstTypeSpecifier>> m_genericArgs;
    RC<AstTypeSpecifier> m_functionReturnType; // optional

    RC<AstTemplateInstantiation> CloneImpl() const
    {
        return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_genericArgs),
            CloneAstNode(m_functionReturnType),
            m_location));
    }
};

} // namespace Hyperion
