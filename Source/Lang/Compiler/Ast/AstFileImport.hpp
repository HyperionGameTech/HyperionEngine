#pragma once

#include <Lang/Compiler/Ast/AstImport.hpp>
#include <Core/Containers/String.hpp>

#include <string>

namespace Hyperion {

HYP_CLASS()
class AstFileImport : public AstImport
{
    HYP_OBJECT_BODY(AstFileImport);

public:
    AstFileImport(
        const String& path,
        const SourceLocation& location);

    virtual ~AstFileImport() override = default;

    const String& GetPath() const
    {
        return m_path;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

protected:
    String m_path;

    Handle<AstFileImport> CloneImpl() const
    {
        return MakeHandle<AstFileImport>(
            m_path,
            m_location);
    }
};

} // namespace Hyperion
