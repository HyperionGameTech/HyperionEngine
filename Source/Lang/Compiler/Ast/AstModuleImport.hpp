#pragma once

#include <Lang/Compiler/Ast/AstImport.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Utilities/Variant.hpp>

namespace Hyperion {

class Identifier;
class SymbolType;

using Symbol = Variant<SharedPtr<Identifier>, const SymbolType*>;

HYP_CLASS()
class AstModuleImportPart : public AstStatement
{
    HYP_OBJECT_BODY(AstModuleImportPart);

public:
    AstModuleImportPart(
        const String& left,
        const Array<SharedPtr<AstModuleImportPart>>& rightParts,
        const SourceLocation& location);
    virtual ~AstModuleImportPart() = default;

    HYP_FORCE_INLINE const String& GetLeft() const
    {
        return m_left;
    }

    HYP_FORCE_INLINE const Array<SharedPtr<AstModuleImportPart>>& GetParts() const
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

    virtual SharedPtr<AstStatement> Clone() const override;

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
    Array<SharedPtr<AstModuleImportPart>> m_rightParts;

    // set while analyzing
    Array<Symbol> m_foundSymbols;
    bool m_pullInModules : 1;

    SharedPtr<AstModuleImportPart> CloneImpl() const
    {
        return SharedPtr<AstModuleImportPart>(new AstModuleImportPart(
            m_left,
            CloneAllAstNodes(m_rightParts),
            m_location));
    }
};

HYP_CLASS()
class AstModuleImport : public AstImport
{
    HYP_OBJECT_BODY(AstModuleImport);

public:
    AstModuleImport(
        const Array<SharedPtr<AstModuleImportPart>>& parts,
        const SourceLocation& location);

    virtual void Visit(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

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
    Array<SharedPtr<AstModuleImportPart>> m_parts;

    SharedPtr<AstModuleImport> CloneImpl() const
    {
        return SharedPtr<AstModuleImport>(new AstModuleImport(
            CloneAllAstNodes(m_parts),
            m_location));
    }
};

} // namespace Hyperion
