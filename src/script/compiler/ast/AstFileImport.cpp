#include <script/compiler/ast/AstFileImport.hpp>
#include <script/compiler/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>

#include <util/string_util.h>

#include <fstream>
#include <iostream>
#include <functional>

namespace hyperion::compiler {

AstFileImport::AstFileImport(
    const std::string &path,
    const SourceLocation &location)
    : AstImport(location),
      m_path(path)
{
}

void AstFileImport::Visit(AstVisitor *visitor, Module *mod)
{
    // find the folder which the current file is in
    std::string dir;
    const size_t index = m_location.GetFileName().find_last_of("/\\");
    if (index != std::string::npos) {
        dir = m_location.GetFileName().substr(0, index) + "/";
    }

    // create relative path
    std::string filepath = dir + m_path;

    AstImport::PerformImport(
        visitor,
        mod,
        filepath
    );
}

Pointer<AstStatement> AstFileImport::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
