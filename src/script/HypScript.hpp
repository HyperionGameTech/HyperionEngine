#pragma once

#include <script/SourceFile.hpp>
#include <script/compiler/ErrorList.hpp>

#include <script/vm/Value.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/reflection/HypData.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {

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
    static inline HypData CreateArgument(T&& item)
    {
        return HypData(HypData(std::forward<T>(item)));
    }

    template <class... Args>
    static inline auto CreateArguments(Args&&... args) -> FixedArray<HypData, sizeof...(Args)>
    {
        return FixedArray<HypData, sizeof...(Args)> { CreateArgument(args)... };
    }

    HypData CallFunctionArgV(Script_Instance* instance, const HypData& value, HypData* args, ArgCount numArgs);

    bool GetFunctionHandle(Script_Instance* instance, const char* name, HypData& outValue);
    bool GetExportedValue(Script_Instance* instance, const char* name, HypData& outValue, bool getReference);

    Script_SymbolTable& GetExportedSymbols(Script_Instance* instance) const;

    /*! \brief Implements OpGetMember in the virtual machine.
     *  Gets a field or method by name and sets `outValue` to the value.
     *  Returns true on found, false otherwise. */
    bool GetMember(Script_Instance* instance, const HypData& targetValue, const char* memberName, HypData& outValue);

    /*! \brief Implements OpSetField in the virtual machine. Sets a field with the name `memberName` to the value held in `value`.
     *  If the field was not found, returns false.
     *  Returns true on success. */
    bool SetField(HypData& targetValue, const char* memberName, HypData&& value);

    template <class... Args>
    HypData CallFunction(Script_Instance* instance, const HypData& value, Args&&... args)
    {
        auto arguments = CreateArguments(std::forward<Args>(args)...);

        return CallFunctionArgV(instance, value, arguments.Data(), arguments.Size());
    }

    void ReadLastReturnValue(Script_Instance* instance, HypData& outValue);

private:
    Pimpl<struct HypScriptImpl> m_impl;
};

} // namespace hyperion
