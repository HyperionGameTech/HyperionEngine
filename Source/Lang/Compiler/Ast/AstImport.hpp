#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/AstIterator.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/CompilationUnit.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {
class ByteReader;
} // namespace Hyperion

namespace Hyperion {

HYP_CLASS(Abstract)
class AstImport : public AstStatement
{
    HYP_OBJECT_BODY(AstImport);

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
        ByteReader*& outStream);

    virtual void Visit(AstVisitor* visitor, Module* mod) override = 0;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override = 0;

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
