#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <core/containers/String.hpp>

namespace Hyperion {
class BufferedReader;
} // namespace Hyperion

namespace Hyperion {

class AstImport : public AstStatement
{
public:
    AstImport(const SourceLocation& location);
    virtual ~AstImport() = default;

    static void CopyModules(
        AstVisitor* visitor,
        Module* modToCopy,
        bool updateTreeLink = false);

    /*! \brief Caller is required to delete the reader's source on success */
    static bool TryOpenFile(
        const String& path,
        BufferedReader& outReader);

    virtual void Visit(AstVisitor* visitor, Module* mod) override = 0;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override = 0;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstImport>());

        return hc;
    }

protected:
    /** The AST iterator that will be used by the imported module */
    AstIterator m_astIterator;

    void PerformImport(
        AstVisitor* visitor,
        Module* mod,
        const String& filepath);
};

} // namespace Hyperion
