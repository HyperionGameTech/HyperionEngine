#pragma once

#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>

#include <core/debug/Debug.hpp>
#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <memory>

namespace hyperion {

class Compiler : public AstVisitor
{
public:
    struct CondInfo
    {
        AstStatement* cond;
        AstStatement* thenPart;
        AstStatement* elsePart;
    };

    struct ExprInfo
    {
        AstExpression* left;
        AstExpression* right;
    };

    static UniquePtr<Buildable> BuildArgumentsStart(
        AstVisitor* visitor,
        Module* mod,
        const Array<RC<AstArgument>>& args,
        uint16& outNumArgs);

    static UniquePtr<Buildable> BuildArgumentsEnd(
        AstVisitor* visitor,
        Module* mod,
        uint16 numArgs);

    static UniquePtr<Buildable> BuildCall(
        AstVisitor* visitor,
        Module* mod,
        const RC<AstExpression>& target,
        uint16 numArgs);

    static UniquePtr<Buildable> LoadMemberFromHash(AstVisitor* visitor, Module* mod, HashCode::ValueType hash);

    static UniquePtr<Buildable> StoreMemberFromHash(AstVisitor* visitor, Module* mod, HashCode::ValueType hash);

    /** Compiler a standard if-then-else statement into the program.
        If the `else` expression is nullptr it will be omitted.
    */
    static UniquePtr<Buildable> CreateConditional(
        AstVisitor* visitor,
        Module* mod,
        AstStatement* cond,
        AstStatement* thenPart,
        AstStatement* elsePart);

    /** Standard evaluation order. Load left into register 0,
        then load right into register 1.
        Rinse and repeat.
    */
    static UniquePtr<Buildable> LoadLeftThenRight(AstVisitor* visitor, Module* mod, ExprInfo info);
    /** Handles the right side before the left side. Used in the case that the
        right hand side is an expression, but the left hand side is just a value.
        If the left hand side is a function call, the right hand side will have to
        be temporarily stored on the stack.
    */
    static UniquePtr<Buildable> LoadRightThenLeft(AstVisitor* visitor, Module* mod, ExprInfo info);
    /** Loads the left hand side and stores it on the stack.
        Then, the right hand side is loaded into a register,
        and the result is computed.
    */
    static UniquePtr<Buildable> LoadLeftAndStore(AstVisitor* visitor, Module* mod, ExprInfo info);
    /** Build a binary operation such as ADD, SUB, MUL, etc. */
    static UniquePtr<Buildable> BuildBinOp(uint8 opcode, AstVisitor* visitor, Module* mod, Compiler::ExprInfo info);
    /** Pops from the stack N times. If N is greater than 1,
        the SUB_SP instruction is generated. Otherwise, the POP
        instruction is generated.
    */
    static UniquePtr<Buildable> PopStack(AstVisitor* visitor, int amt);

public:
    Compiler(AstIterator* astIterator, CompilationUnit* compilationUnit);
    Compiler(const Compiler& other);

    UniquePtr<BytecodeChunk> Compile();
};

} // namespace hyperion
