#include <script/compiler/ast/AstFileImport.hpp>
#include <script/SourceFile.hpp>
#include <script/compiler/Lexer.hpp>
#include <script/compiler/Parser.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>
#include <script/compiler/Optimizer.hpp>

#include <util/StringUtil.hpp>

#include <Types.hpp>

#include <fstream>
#include <iostream>

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
    std::string dir = m_location.GetFileName();
    const SizeType index = dir.find_last_of("/\\");

    if (index != std::string::npos) {
        dir = dir.substr(0, index) + "/";
    }

    // create relative path
    std::string filepath = dir + m_path;

    AstImport::PerformImport(
        visitor,
        mod,
        filepath
    );
}

RC<AstStatement> AstFileImport::Clone() const
{
    return CloneImpl();
}

} // namespace hyperion::compiler
