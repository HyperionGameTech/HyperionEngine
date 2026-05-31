#pragma once

#include <Lang/vm/BytecodeStream.hpp>
#include <Lang/compiler/emit/Instruction.hpp>
#include <Lang/compiler/emit/BytecodeUtil.hpp>
#include <Core/containers/String.hpp>
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
