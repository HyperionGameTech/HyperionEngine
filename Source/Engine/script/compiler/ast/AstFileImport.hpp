#pragma once

#include <script/compiler/ast/AstImport.hpp>
#include <core/containers/String.hpp>

#include <string>

namespace Hyperion {

class AstFileImport : public AstImport
{
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

    virtual RC<AstStatement> Clone() const override;

protected:
    String m_path;

    RC<AstFileImport> CloneImpl() const
    {
        return RC<AstFileImport>(new AstFileImport(
            m_path,
            m_location));
    }
};

} // namespace Hyperion
