#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <Lang/SourceFile.hpp>
#include <Lang/compiler/ErrorList.hpp>

#include <Lang/vm/Value.hpp>

#include <Core/containers/FixedArray.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/memory/Pimpl.hpp>

#ifndef HYP_SCRIPT_API
#ifdef HYP_BUILD_ENGINE
#define HYP_SCRIPT_API ENGINE_API
#elif defined(HYP_BUILD_CORE)
#define HYP_SCRIPT_API CORE_API
#else
#define HYP_SCRIPT_API
#endif
#endif

namespace Hyperion {

class VirtualMachine;
class SymbolTable;
class InstructionStream;

struct ScriptInstance;

struct HypScriptCompileParams
{
    TSet<FilePath> scanPaths;
};

namespace HypScript
{

VirtualMachine* GetVM();

void Initialize();
void Shutdown();

HYP_SCRIPT_API void DestroyScript(ScriptInstance* instance);

HYP_NODISCARD HYP_SCRIPT_API ScriptInstance* Compile(
    SourceFile& sourceFile,
    ErrorList& outErrorList,
    const HypScriptCompileParams& params = {});

HYP_NODISCARD HYP_SCRIPT_API ScriptInstance* CreateFromBytecode(ConstByteView view);

HYP_SCRIPT_API void WriteBytecodeToStream(ScriptInstance* instance, ByteWriter& stream);

HYP_SCRIPT_API void Run(ScriptInstance* instance);

void CollectGarbage();

HYP_SCRIPT_API InstructionStream* Decompile(ScriptInstance* instance, std::ostream* os = nullptr);

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

HYP_SCRIPT_API BoxedValue CallFunctionArgV(ScriptInstance* instance, const BoxedValue& value, BoxedValue* args, uint8 numArgs);

HYP_SCRIPT_API bool GetFunctionHandle(ScriptInstance* instance, const char* name, BoxedValue& outValue);
HYP_SCRIPT_API bool GetExportedValue(ScriptInstance* instance, const char* name, BoxedValue& outValue, bool getReference);

HYP_SCRIPT_API SymbolTable& GetExportedSymbols(ScriptInstance* instance);

HYP_SCRIPT_API bool GetMember(ScriptInstance* instance, const BoxedValue& targetValue, const char* memberName, BoxedValue& outValue);

HYP_SCRIPT_API bool SetField(BoxedValue& targetValue, const char* memberName, BoxedValue&& value);

template <class... Args>
BoxedValue CallFunction(ScriptInstance* instance, const BoxedValue& value, Args&&... args)
{
    auto arguments = CreateArguments(std::forward<Args>(args)...);

    return CallFunctionArgV(instance, value, arguments.Data(), arguments.Size());
}

HYP_SCRIPT_API void ReadLastReturnValue(ScriptInstance* instance, BoxedValue& outValue);

} // namespace HypScript
} // namespace Hyperion
