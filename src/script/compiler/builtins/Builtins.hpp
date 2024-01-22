#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <script/SourceLocation.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>

#include <core/lib/HashMap.hpp>
#include <core/lib/String.hpp>

#include <map>
#include <string>
#include <memory>

namespace hyperion::compiler {

class AstVisitor;

class Builtins
{
public:
    Builtins();

    /** This will analyze the builtins, and add them to the syntax tree.
     */
    void Visit(AstVisitor *visitor, CompilationUnit *unit);

private:
    static const SourceLocation BUILTIN_SOURCE_LOCATION;
};

} // namespace hyperion::compiler

#endif
