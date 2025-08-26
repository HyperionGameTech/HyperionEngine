#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/Enums.hpp>
#include <core/containers/String.hpp>

#include <string>

namespace hyperion::compiler {

// forward declaration
class Scope;

struct AstIdentifierProperties
{
    RC<Identifier> m_identifier = nullptr;

    IdentifierType m_identifierType = IDENTIFIER_TYPE_UNKNOWN;

    bool m_isInFunction = false;

    int m_depth = 0;
    Scope* m_functionScope = nullptr;

    // if the found identifier was a type...
    SymbolTypeRef m_foundType = nullptr;

    // getters & setters
    RC<Identifier>& GetIdentifier()
    {
        return m_identifier;
    }

    const RC<Identifier>& GetIdentifier() const
    {
        return m_identifier;
    }

    void SetIdentifier(const RC<Identifier>& identifier)
    {
        m_identifier = identifier;
    }

    IdentifierType GetIdentifierType() const
    {
        return m_identifierType;
    }

    void SetIdentifierType(IdentifierType identifierType)
    {
        m_identifierType = identifierType;
    }

    bool IsInFunction() const
    {
        return m_isInFunction;
    }

    int GetDepth() const
    {
        return m_depth;
    }

    Scope* GetFunctionScope() const
    {
        return m_functionScope;
    }
};

class AstIdentifier : public AstExpression
{
public:
    AstIdentifier(
        const String& name,
        const SourceLocation& location);

    virtual ~AstIdentifier() override = default;

    void PerformLookup(AstVisitor* visitor, Module* mod);
    void CheckInFunction(AstVisitor* visitor, Module* mod);

    AstIdentifierProperties& GetProperties()
    {
        return m_properties;
    }
    const AstIdentifierProperties& GetProperties() const
    {
        return m_properties;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override = 0;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override = 0;

    virtual RC<AstStatement> Clone() const override = 0;

    virtual Tribool IsTrue() const override = 0;
    virtual bool MayHaveSideEffects() const override = 0;

    virtual SymbolTypeRef GetExprType() const override = 0;
    virtual SymbolTypeRef GetHeldType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

    virtual const String& GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstExpression::GetHashCode().Add(TypeName<AstIdentifier>()).Add(m_name);
    }

protected:
    String m_name;

    AstIdentifierProperties m_properties;

    int GetStackOffset(int stackSize) const;
};

} // namespace hyperion::compiler
