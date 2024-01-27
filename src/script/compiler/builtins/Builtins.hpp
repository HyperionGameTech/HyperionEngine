#ifndef BUILTINS_HPP
#define BUILTINS_HPP

#include <script/SourceLocation.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>

#include <core/lib/HashMap.hpp>
#include <core/lib/String.hpp>

#include <map>
#include <string>
#include <memory>

namespace hyperion::compiler {

class AstVisitor;
class CompilationUnit;

class Builtins
{
public:
    Builtins(CompilationUnit *unit);

    RC<AstVariableDeclaration> FindVariable(const String &name) const
    {
        auto it = m_vars.FindIf([&name](const auto &var)
        {
            if (!var) {
                return false;
            }

            return var->GetName() == name;
        });

        if (it == m_vars.End()) {
            return nullptr;
        }

        return *it;
    }

    /** This will analyze the builtins, and add them to the syntax tree.
     */
    void Visit(AstVisitor *visitor);

private:
    static const SourceLocation BUILTIN_SOURCE_LOCATION;

    CompilationUnit                     *m_unit;
    Array<RC<AstVariableDeclaration>>   m_vars;
};

} // namespace hyperion::compiler

#endif
