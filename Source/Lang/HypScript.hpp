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
ScriptInstance* GetGlobalInstance();

void Initialize();
void Shutdown();

void DestroyScript(ScriptInstance* instance);

HYP_NODISCARD ScriptInstance* Compile(
    SourceFile& sourceFile,
    ErrorList& outErrorList,
    const HypScriptCompileParams& params = {});

HYP_NODISCARD ScriptInstance* CreateFromBytecode(ConstByteView view);

void WriteBytecodeToStream(ScriptInstance* instance, ByteWriter& stream);

void Run(ScriptInstance* instance);

void CollectGarbage();

InstructionStream* Decompile(ScriptInstance* instance, std::ostream* os = nullptr);

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

BoxedValue CallFunctionArgV(ScriptInstance* instance, const BoxedValue& value, BoxedValue* args, uint8 numArgs);

bool GetFunctionHandle(ScriptInstance* instance, const char* name, BoxedValue& outValue);
bool GetExportedValue(ScriptInstance* instance, const char* name, BoxedValue& outValue, bool getReference);

SymbolTable& GetExportedSymbols(ScriptInstance* instance);

/*! \brief Implements OpGetMember in the virtual machine.
    *  Gets a field or method by name and sets `outValue` to the value.
    *  Returns true on found, false otherwise. */
bool GetMember(ScriptInstance* instance, const BoxedValue& targetValue, const char* memberName, BoxedValue& outValue);

/*! \brief Implements OpSetField in the virtual machine. Sets a field with the name `memberName` to the value held in `value`.
    *  If the field was not found, returns false.
    *  Returns true on success. */
bool SetField(BoxedValue& targetValue, const char* memberName, BoxedValue&& value);

template <class... Args>
BoxedValue CallFunction(ScriptInstance* instance, const BoxedValue& value, Args&&... args)
{
    auto arguments = CreateArguments(std::forward<Args>(args)...);

    return CallFunctionArgV(instance, value, arguments.Data(), arguments.Size());
}

void ReadLastReturnValue(ScriptInstance* instance, BoxedValue& outValue);

} // namespace HypScript
} // namespace Hyperion
