#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <script/SourceLocation.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>

#include <map>
#include <string>
#include <memory>

namespace hyperion::compiler {

class Builtins {
public:
    Builtins();

    const AstIterator &GetAst() const { return m_ast; }

    /** This will analyze the builtins, and add them to the syntax tree.
     */
    void Visit(CompilationUnit *unit);

    /** This will return bytecode containing builtins
     */
    std::unique_ptr<BytecodeChunk> Build(CompilationUnit *unit);

private:
    static const SourceLocation BUILTIN_SOURCE_LOCATION;

    std::map<std::string, std::shared_ptr<AstExpression>> m_vars;
    AstIterator m_ast;
};

} // namespace hyperion::compiler

#endif
