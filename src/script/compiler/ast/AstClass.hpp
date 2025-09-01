#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstTypeRef.hpp>

namespace hyperion::compiler {

class AstClass : public AstExpression
{
public:
    AstClass(
        const String& name,
        const RC<AstTypeSpecifier>& baseSpec,
        const Array<RC<AstVariableDeclaration>>& dataMembers,
        const Array<RC<AstVariableDeclaration>>& functionMembers,
        const Array<RC<AstVariableDeclaration>>& staticMembers,
        bool isProxyClass,
        const SourceLocation& location);

    AstClass(
        const String& name,
        const SymbolTypeRef& baseType,
        const Array<RC<AstVariableDeclaration>>& dataMembers,
        const Array<RC<AstVariableDeclaration>>& functionMembers,
        const Array<RC<AstVariableDeclaration>>& staticMembers,
        bool isProxyClass,
        const SourceLocation& location);

    AstClass(
        const String& name,
        const RC<AstTypeSpecifier>& baseSpec,
        const Array<RC<AstVariableDeclaration>>& dataMembers,
        const Array<RC<AstVariableDeclaration>>& functionMembers,
        const Array<RC<AstVariableDeclaration>>& staticMembers,
        const SymbolTypeRef& enumUnderlyingType,
        bool isProxyClass,
        const SourceLocation& location);

    virtual ~AstClass() override = default;

    /** enable setting to that variable declarations can change the type name */
    void SetName(const String& name)
    {
        m_name = name;
    }

    Array<RC<AstVariableDeclaration>>& GetDataMembers()
    {
        return m_dataMembers;
    }

    const Array<RC<AstVariableDeclaration>>& GetDataMembers() const
    {
        return m_dataMembers;
    }

    Array<RC<AstVariableDeclaration>>& GetFunctionMembers()
    {
        return m_functionMembers;
    }

    const Array<RC<AstVariableDeclaration>>& GetFunctionMembers() const
    {
        return m_functionMembers;
    }

    Array<RC<AstVariableDeclaration>>& GetStaticMembers()
    {
        return m_staticMembers;
    }

    const Array<RC<AstVariableDeclaration>>& GetStaticMembers() const
    {
        return m_functionMembers;
    }

    bool IsEnum() const
    {
        return m_enumUnderlyingType != nullptr;
    }

    bool IsProxyClass() const
    {
        return m_isProxyClass;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override;
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual SymbolTypeRef GetExprType() const override;
    virtual SymbolTypeRef GetHeldType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual const String& GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstClass>());
        hc.Add(m_name);
        hc.Add(m_baseSpec ? m_baseSpec->GetHashCode() : HashCode());

        for (auto& member : m_dataMembers)
        {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto& member : m_functionMembers)
        {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        for (auto& member : m_staticMembers)
        {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        if (m_enumUnderlyingType != nullptr)
        {
            hc.Add(m_enumUnderlyingType->GetHashCode());
        }

        hc.Add(m_isProxyClass);

        return hc;
    }

protected:
    String m_name;
    RC<AstTypeSpecifier> m_baseSpec;
    SymbolTypeRef m_baseType;
    Array<RC<AstVariableDeclaration>> m_dataMembers;
    Array<RC<AstVariableDeclaration>> m_functionMembers;
    Array<RC<AstVariableDeclaration>> m_staticMembers;
    SymbolTypeRef m_enumUnderlyingType;
    bool m_isProxyClass;

    SymbolTypeRef m_symbolType;

    RC<AstTypeRef> m_typeRef;
    Array<RC<AstVariableDeclaration>> m_outsideMembers;
    Array<RC<AstVariableDeclaration>> m_combinedMembers;
    bool m_isVisited;

    RC<AstClass> CloneImpl() const
    {
        if (m_baseType != nullptr)
        {
            return RC<AstClass>(new AstClass(
                m_name,
                m_baseType,
                CloneAllAstNodes(m_dataMembers),
                CloneAllAstNodes(m_functionMembers),
                CloneAllAstNodes(m_staticMembers),
                m_isProxyClass,
                m_location));
        }

        return RC<AstClass>(new AstClass(
            m_name,
            CloneAstNode(m_baseSpec),
            CloneAllAstNodes(m_dataMembers),
            CloneAllAstNodes(m_functionMembers),
            CloneAllAstNodes(m_staticMembers),
            m_enumUnderlyingType,
            m_isProxyClass,
            m_location));
    }
};

} // namespace hyperion::compiler
