#ifndef ACE_C_HPP
#define ACE_C_HPP

#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>

#include <util/utf8.hpp>

#include <memory>

namespace hyperion {
namespace compiler {

std::unique_ptr<BytecodeChunk> BuildSourceFile(
    const utf::Utf8String &filename,
    const utf::Utf8String &out_filename);

std::unique_ptr<BytecodeChunk> BuildSourceFile(
    const utf::Utf8String &filename,
    const utf::Utf8String &out_filename, CompilationUnit &compilation_unit);

void DecompileBytecodeFile(const utf::Utf8String &filename,
    const utf::Utf8String &out_filename);

} // namespace compiler
} // namespace hyperion

#endif
