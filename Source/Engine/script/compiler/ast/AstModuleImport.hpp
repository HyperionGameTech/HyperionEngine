#pragma once

#include <script/compiler/ast/AstImport.hpp>

#include <Core/containers/String.hpp>

#include <Core/utilities/Variant.hpp>

namespace Hyperion {

class Identifier;
class SymbolType;

using Symbol = Variant<RC<Identifier>, const SymbolType*>;

class AstModuleImportPart : public AstStatement
{
public:
    AstModuleImportPart(
        const String& left,
        const Array<RC<AstModuleImportPart>>& rightParts,
        const SourceLocation& location);
    virtual ~AstModuleImportPart() = default;

    HYP_FORCE_INLINE const String& GetLeft() const
    {
        return m_left;
    }

    HYP_FORCE_INLINE const Array<RC<AstModuleImportPart>>& GetParts() const
    {
        return m_rightParts;
    }

    HYP_FORCE_INLINE void SetPullInModules(bool pullInModules)
    {
        m_pullInModules = pullInModules;
    }

    HYP_FORCE_INLINE const Array<Symbol>& GetFoundSymbols() const
    {
        return m_foundSymbols;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstModuleImportPart>());
        hc.Add(m_left);

        for (auto& part : m_rightParts)
        {
            hc.Add(part ? part->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    String m_left;
    Array<RC<AstModuleImportPart>> m_rightParts;

    // set while analyzing
    Array<Symbol> m_foundSymbols;
    bool m_pullInModules : 1;

    RC<AstModuleImportPart> CloneImpl() const
    {
        return RC<AstModuleImportPart>(new AstModuleImportPart(
            m_left,
            CloneAllAstNodes(m_rightParts),
            m_location));
    }
};

class AstModuleImport : public AstImport
{
public:
    AstModuleImport(
        const Array<RC<AstModuleImportPart>>& parts,
        const SourceLocation& location);

    virtual void Visit(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstImport::GetHashCode().Add(TypeName<AstModuleImport>());

        for (auto& part : m_parts)
        {
            hc.Add(part ? part->GetHashCode() : HashCode());
        }

        return hc;
    }

protected:
    Array<RC<AstModuleImportPart>> m_parts;

    RC<AstModuleImport> CloneImpl() const
    {
        return RC<AstModuleImport>(new AstModuleImport(
            CloneAllAstNodes(m_parts),
            m_location));
    }
};

} // namespace Hyperion
