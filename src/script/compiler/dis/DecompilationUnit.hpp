#ifndef DECOMPILATION_UNIT_HPP
#define DECOMPILATION_UNIT_HPP

#include <script/vm/BytecodeStream.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <util/UTF8.hpp>

#include <memory>

namespace hyperion::compiler {

class DecompilationUnit {
public:
    DecompilationUnit();
    DecompilationUnit(const DecompilationUnit &other) = delete;

    void DecodeNext(
        uint8_t code,
        hyperion::vm::BytecodeStream &bs,
        InstructionStream &is,
        utf::utf8_ostream *os = nullptr);

    InstructionStream Decompile(
        hyperion::vm::BytecodeStream &bs,
        utf::utf8_ostream *os = nullptr);
};

} // namespace hyperion::compiler

#endif
