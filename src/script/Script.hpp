#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include <script/ScriptApi.hpp>

#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/vm/BytecodeStream.hpp>
#include <script/vm/VM.hpp>

#include <types.h>

#include <util/utf8.hpp>

#include <memory>

namespace hyperion {

using namespace compiler;
using namespace vm;

class APIInstance;

class Script {
public:
    using Bytes = std::vector<UByte>;

    Script(const SourceFile &source_file);
    ~Script() = default;

    const ErrorList &GetErrors() const { return m_errors; }

    bool IsBaked() const               { return !m_baked_bytes.empty(); }
    bool IsCompiled() const            { return m_bytecode_chunk != nullptr; }

    bool Compile(APIInstance &api_instance);

    InstructionStream Decompile(utf::utf8_ostream *os = nullptr) const;

    void Bake();
    void Bake(BuildParams &build_params);

    void Run();

private:
    SourceFile                     m_source_file;
    CompilationUnit                m_compilation_unit;
    ErrorList                      m_errors;

    std::unique_ptr<BytecodeChunk> m_bytecode_chunk;

    Bytes                          m_baked_bytes;

    VM                             m_vm;
};

} // namespace hyperion

#endif
