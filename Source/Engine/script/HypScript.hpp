#pragma once

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>

#include <script/vm/Value.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/reflection/BoxedValue.hpp>

#include <core/memory/Pimpl.hpp>

namespace Hyperion {

class HypScript;
class Script_Interpreter;
class Script_SymbolTable;
class InstructionStream;

struct Script_Instance;

class HypScript
{
public:
    using ArgCount = uint16;

    static HypScript& GetInstance();

    HypScript();
    HypScript(const HypScript& other) = delete;
    HypScript& operator=(const HypScript& other) = delete;
    ~HypScript();

    Script_Interpreter* GetVM() const;
    Script_Instance* GetGlobalInstance() const;

    void Initialize();

    void DestroyScript(Script_Instance* instance);

    Script_Instance* Compile(SourceFile& sourceFile, ErrorList& outErrorList);
    InstructionStream* Decompile(Script_Instance* instance, std::ostream* os = nullptr) const;

    void Run(Script_Instance* instance);

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

    BoxedValue CallFunctionArgV(Script_Instance* instance, const BoxedValue& value, BoxedValue* args, ArgCount numArgs);

    bool GetFunctionHandle(Script_Instance* instance, const char* name, BoxedValue& outValue);
    bool GetExportedValue(Script_Instance* instance, const char* name, BoxedValue& outValue, bool getReference);

    Script_SymbolTable& GetExportedSymbols(Script_Instance* instance) const;

    /*! \brief Implements OpGetMember in the virtual machine.
     *  Gets a field or method by name and sets `outValue` to the value.
     *  Returns true on found, false otherwise. */
    bool GetMember(Script_Instance* instance, const BoxedValue& targetValue, const char* memberName, BoxedValue& outValue);

    /*! \brief Implements OpSetField in the virtual machine. Sets a field with the name `memberName` to the value held in `value`.
     *  If the field was not found, returns false.
     *  Returns true on success. */
    bool SetField(BoxedValue& targetValue, const char* memberName, BoxedValue&& value);

    template <class... Args>
    BoxedValue CallFunction(Script_Instance* instance, const BoxedValue& value, Args&&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        return CallFunctionArgV(instance, value, arguments.Data(), arguments.Size());
    }

    void ReadLastReturnValue(Script_Instance* instance, BoxedValue& outValue);

private:
    Pimpl<struct HypScriptImpl> m_impl;
};

} // namespace Hyperion
