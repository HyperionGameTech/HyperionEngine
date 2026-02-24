#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>

#include <script/vm/Value.hpp>

#include <Core/containers/FixedArray.hpp>

#include <Core/reflection/BoxedValue.hpp>

#include <Core/memory/Pimpl.hpp>

namespace Hyperion {

class HypScript;
class VirtualMachine;
class Script_SymbolTable;
class InstructionStream;

struct ScriptInstance;

class HypScript
{
public:
    using ArgCount = uint16;

    static HypScript& GetInstance();

    HypScript();
    HypScript(const HypScript& other) = delete;
    HypScript& operator=(const HypScript& other) = delete;
    ~HypScript();

    VirtualMachine* GetVM() const;
    ScriptInstance* GetGlobalInstance() const;

    void Initialize();

    void DestroyScript(ScriptInstance* instance);

    ScriptInstance* Compile(SourceFile& sourceFile, ErrorList& outErrorList);
    InstructionStream* Decompile(ScriptInstance* instance, std::ostream* os = nullptr) const;

    void Run(ScriptInstance* instance);

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

    BoxedValue CallFunctionArgV(ScriptInstance* instance, const BoxedValue& value, BoxedValue* args, ArgCount numArgs);

    bool GetFunctionHandle(ScriptInstance* instance, const char* name, BoxedValue& outValue);
    bool GetExportedValue(ScriptInstance* instance, const char* name, BoxedValue& outValue, bool getReference);

    Script_SymbolTable& GetExportedSymbols(ScriptInstance* instance) const;

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

private:
    Pimpl<struct HypScriptImpl> m_impl;
};

} // namespace Hyperion
