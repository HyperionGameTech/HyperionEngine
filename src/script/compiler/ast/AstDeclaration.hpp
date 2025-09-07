#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/Identifier.hpp>

#include <string>

namespace hyperion {

class AstDeclaration : public AstStatement
{
public:
    AstDeclaration(
        const String& name,
        IdentifierFlagBits flags,
        const SourceLocation& location);
    virtual ~AstDeclaration() = default;

    void SetName(const String& name)
    {
        m_name = name;
    }

    const RC<Identifier>& GetIdentifier() const
    {
        return m_identifier;
    }

    bool IsConst() const
    {
        return m_flags & IdentifierFlags::FLAG_CONST;
    }

    bool IsRef() const
    {
        return m_flags & IdentifierFlags::FLAG_REF;
    }

    IdentifierFlagBits GetIdentifierFlags() const
    {
        return m_flags;
    }

    void SetIdentifierFlags(IdentifierFlagBits flags)
    {
        m_flags = flags;
    }

    void ApplyIdentifierFlags(IdentifierFlagBits flags, bool set = true)
    {
        if (set)
        {
            m_flags |= flags;
        }
        else
        {
            m_flags &= ~flags;
        }
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override = 0;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override = 0;

    virtual RC<AstStatement> Clone() const override = 0;

    virtual const String& GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstDeclaration>());
        hc.Add(m_name);
        hc.Add(m_flags);

        return hc;
    }

protected:
    String m_name;
    RC<Identifier> m_identifier;
    IdentifierFlagBits m_flags;

private:
    bool m_isVisited = false;
};

} // namespace hyperion
