#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>

#include <system/Debug.hpp>

#include <Types.hpp>

#include <memory>

namespace hyperion::compiler {

class Compiler : public AstVisitor
{
public:
    struct CondInfo
    {
        AstStatement *cond;
        AstStatement *then_part;
        AstStatement *else_part;
    };

    struct ExprInfo
    {
        AstExpression *left;
        AstExpression *right;
    };

    static std::unique_ptr<Buildable> BuildArgumentsStart(
        AstVisitor *visitor,
        Module *mod,
        const Array<RC<AstArgument>> &args
    );

    static std::unique_ptr<Buildable> BuildArgumentsEnd(
        AstVisitor *visitor,
        Module *mod,
        UInt8 nargs
    );

    static std::unique_ptr<Buildable> BuildCall(
        AstVisitor *visitor,
        Module *mod,
        const RC<AstExpression> &target,
        UInt8 nargs
    );

    static std::unique_ptr<Buildable> LoadMemberFromHash(AstVisitor *visitor, Module *mod, UInt32 hash);

    static std::unique_ptr<Buildable> StoreMemberFromHash(AstVisitor *visitor, Module *mod, UInt32 hash);

    static std::unique_ptr<Buildable> LoadMemberAtIndex(AstVisitor *visitor, Module *mod, UInt8 index);

    static std::unique_ptr<Buildable> StoreMemberAtIndex(AstVisitor *visitor, Module *mod, UInt8 index);

    /** Compiler a standard if-then-else statement into the program.
        If the `else` expression is nullptr it will be omitted.
    */
    static std::unique_ptr<Buildable> CreateConditional(
        AstVisitor *visitor,
        Module *mod,
        AstStatement *cond,
        AstStatement *then_part,
        AstStatement *else_part
    );

    /** Standard evaluation order. Load left into register 0,
        then load right into register 1.
        Rinse and repeat.
    */
    static std::unique_ptr<Buildable> LoadLeftThenRight(AstVisitor *visitor, Module *mod, ExprInfo info);
    /** Handles the right side before the left side. Used in the case that the
        right hand side is an expression, but the left hand side is just a value.
        If the left hand side is a function call, the right hand side will have to
        be temporarily stored on the stack.
    */
    static std::unique_ptr<Buildable> LoadRightThenLeft(AstVisitor *visitor, Module *mod, ExprInfo info);
    /** Loads the left hand side and stores it on the stack.
        Then, the right hand side is loaded into a register,
        and the result is computed.
    */
    static std::unique_ptr<Buildable> LoadLeftAndStore(AstVisitor *visitor, Module *mod, ExprInfo info);
    /** Build a binary operation such as ADD, SUB, MUL, etc. */
    static std::unique_ptr<Buildable> BuildBinOp(UInt8 opcode, AstVisitor *visitor, Module *mod, Compiler::ExprInfo info);
    /** Pops from the stack N times. If N is greater than 1,
        the SUB_SP instruction is generated. Otherwise, the POP
        instruction is generated.
    */
    static std::unique_ptr<Buildable> PopStack(AstVisitor *visitor, Int amt);

public:
    Compiler(AstIterator *ast_iterator, CompilationUnit *compilation_unit);
    Compiler(const Compiler &other);

    std::unique_ptr<BytecodeChunk> Compile();
};

} // namespace hyperion::compiler

#endif
