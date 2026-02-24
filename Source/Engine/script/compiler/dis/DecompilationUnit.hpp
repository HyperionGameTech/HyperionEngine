#pragma once

#include <script/vm/BytecodeStream.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
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
