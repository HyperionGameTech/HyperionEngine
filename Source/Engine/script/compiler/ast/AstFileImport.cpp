#include <script/compiler/ast/AstFileImport.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/Types.hpp>

#include <fstream>
#include <iostream>

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
