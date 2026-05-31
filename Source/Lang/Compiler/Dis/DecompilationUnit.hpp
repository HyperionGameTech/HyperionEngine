#pragma once

#include <Lang/VM/BytecodeStream.hpp>
#include <Lang/Compiler/Emit/Instruction.hpp>
#include <Lang/Compiler/Emit/BytecodeUtil.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Unicode.hpp>

#include <memory>

namespace Hyperion {

class InstructionStream;

class DecompilationUnit
{
public:
    DecompilationUnit();
    DecompilationUnit(const DecompilationUnit& other) = delete;

    void DecodeNext(uint8 code, BytecodeStream& bs, InstructionStream& is, std::ostream* os = nullptr);
    InstructionStream* Decompile(BytecodeStream& bs, std::ostream* os = nullptr);
};

} // namespace Hyperion
