#include <Lang/Compiler/Ast/AstFileImport.hpp>
#include <Lang/SourceFile.hpp>
#include <Lang/Compiler/Lexer.hpp>
#include <Lang/Compiler/Parser.hpp>
#include <Lang/Compiler/SemanticAnalyzer.hpp>
#include <Lang/Compiler/Optimizer.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Types.hpp>

#include <fstream>
#include <iostream>

#include <AstFileImport.generated.inl>

namespace Hyperion {

AstFileImport::AstFileImport(
    const String& path,
    const SourceLocation& location)
    : AstImport(location),
      m_path(path)
{
}

void AstFileImport::Visit(AstVisitor* visitor, Module* mod)
{
    // find the folder which the current file is in
    String dir = m_location.GetFileName();

    size_t slashIndex = dir.FindLastIndex('/');
    slashIndex = (slashIndex == String::NotFound)
        ? dir.FindLastIndex('\\')
        : MathUtil::Max(slashIndex, dir.FindLastIndex('\\'));

    if (slashIndex != String::NotFound)
    {
        dir = dir.Substr(0, slashIndex);
    }

    // create relative path
    String filepath = FilePath::Join(dir, m_path);

    AstImport::PerformImport(visitor, mod, filepath);
}

RC<AstStatement> AstFileImport::Clone() const
{
    return CloneImpl();
}

} // namespace Hyperion
