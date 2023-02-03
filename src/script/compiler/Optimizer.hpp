#ifndef OPTIMIZER_HPP
#define OPTIMIZER_HPP

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/ast/AstExpression.hpp>

#include <memory>

namespace hyperion::compiler {

// forward declarations
class AstConstant;

class Optimizer : public AstVisitor {
public:
    /** Attemps to evaluate the optimized expression at compile-time. */
    static RC<AstConstant> ConstantFold(
        RC<AstExpression> &left,
        RC<AstExpression> &right, 
        Operators op_type,
        AstVisitor *visitor);

    /** Attemps to reduce a variable that is const literal to the actual value. */
    static RC<AstExpression> OptimizeExpr(
        const RC<AstExpression> &expr,
        AstVisitor *visitor,
        Module *mod);

public:
    Optimizer(AstIterator *ast_iterator,
        CompilationUnit *compilation_unit);
    Optimizer(const Optimizer &other);

    void Optimize(bool expect_module_decl = true);

private:
    void OptimizeInner();
};

} // namespace hyperion::compiler

#endif
