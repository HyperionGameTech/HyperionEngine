#pragma once

#include <Lang/Compiler/AstVisitor.hpp>
#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/Operator.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>

#include <memory>

namespace Hyperion {

// forward declarations
class AstConstant;

class Optimizer : public AstVisitor
{
public:
    /** Attemps to evaluate the optimized expression at compile-time. */
    static RC<AstConstant> ConstantFold(
        AstExpression* left,
        AstExpression* right,
        Operators opType,
        AstVisitor* visitor);

    /** Attemps to reduce a variable that is const literal to the actual value. */
    static RC<AstExpression> OptimizeExpr(
        const RC<AstExpression>& expr,
        AstVisitor* visitor,
        Module* mod);

public:
    Optimizer(AstIterator* astIterator,
        CompilationUnit* compilationUnit);
    Optimizer(const Optimizer& other);

    void Optimize(bool expectModuleDecl = true);

private:
    void OptimizeInner();
};

} // namespace Hyperion
