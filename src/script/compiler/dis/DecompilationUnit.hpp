#pragma once

#include <script/vm/Stream.hpp>
#include <script/compiler/emit/Instruction.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>
#include <core/containers/String.hpp>
#include <util/UTF8.hpp>

#include <memory>

namespace hyperion {

class InstructionStream;

class DecompilationUnit
{
public:
    DecompilationUnit();
    DecompilationUnit(const DecompilationUnit& other) = delete;

    void DecodeNext(uint8 code, Script_Stream& bs, InstructionStream& is, std::ostream* os = nullptr);
    InstructionStream* Decompile(Script_Stream& bs, std::ostream* os = nullptr);
};

} // namespace hyperion
