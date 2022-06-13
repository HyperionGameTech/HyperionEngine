#ifndef AST_TYPE_HOLDER_HPP
#define AST_TYPE_HOLDER_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/enums.hpp>

#include <string>

namespace hyperion::compiler {

class AstTypeHolder : public AstExpression {
public:
    AstTypeHolder(const SourceLocation &location);
    virtual ~AstTypeHolder() = default;

    virtual SymbolTypePtr_t GetHeldType() const = 0;
};

} // namespace hyperion::compiler

#endif
