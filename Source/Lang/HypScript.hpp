#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <Lang/SourceFile.hpp>
#include <Lang/Compiler/ErrorList.hpp>

#include <Lang/VM/Value.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Memory/Pimpl.hpp>

namespace Hyperion {

class VirtualMachine;
class SymbolTable;
class InstructionStream;

struct ScriptInstance;

struct HypScriptCompileParams
{
    Set<FilePath> scanPaths;
};

namespace HypScript {

VirtualMachine* GetVM();

SCRIPT_API void Initialize();
SCRIPT_API void Shutdown();

SCRIPT_API void DestroyScript(ScriptInstance* instance);

HYP_NODISCARD SCRIPT_API ScriptInstance* Compile(
    SourceFile& sourceFile,
    ErrorList& outErrorList,
    const HypScriptCompileParams& params = {});

HYP_NODISCARD SCRIPT_API ScriptInstance* CreateFromBytecode(ConstByteView view);

SCRIPT_API void WriteBytecodeToStream(ScriptInstance* instance, ByteWriter& stream);

SCRIPT_API void Run(ScriptInstance* instance);

SCRIPT_API void CollectGarbage();

SCRIPT_API InstructionStream* Decompile(ScriptInstance* instance, std::ostream* os = nullptr);

template <class T>
static inline BoxedValue CreateArgument(T&& item)
{
    return BoxedValue(BoxedValue(std::forward<T>(item)));
}

template <class... Args>
static inline auto CreateArguments(Args&&... args) -> FixedArray<BoxedValue, sizeof...(Args)>
{
    return FixedArray<BoxedValue, sizeof...(Args)> { CreateArgument(args)... };
}

SCRIPT_API BoxedValue CallFunctionArgV(ScriptInstance* instance, const BoxedValue& value, BoxedValue* args, uint8 numArgs);

SCRIPT_API bool GetFunctionHandle(ScriptInstance* instance, const char* name, BoxedValue& outValue);
SCRIPT_API bool GetExportedValue(ScriptInstance* instance, const char* name, BoxedValue& outValue, bool getReference);

SCRIPT_API SymbolTable& GetExportedSymbols(ScriptInstance* instance);

SCRIPT_API bool GetMember(ScriptInstance* instance, const BoxedValue& targetValue, const char* memberName, BoxedValue& outValue);

SCRIPT_API bool SetField(BoxedValue& targetValue, const char* memberName, BoxedValue&& value);

template <class... Args>
BoxedValue CallFunction(ScriptInstance* instance, const BoxedValue& value, Args&&... args)
{
    auto arguments = CreateArguments(std::forward<Args>(args)...);

    return CallFunctionArgV(instance, value, arguments.Data(), arguments.Size());
}

SCRIPT_API void ReadLastReturnValue(ScriptInstance* instance, BoxedValue& outValue);

} // namespace HypScript
} // namespace Hyperion
