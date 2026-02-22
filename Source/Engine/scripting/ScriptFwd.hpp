#pragma once

#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <Core/utilities/EnumFlags.hpp>

namespace Hyperion {

namespace dotnet {
class ManagedClass;
class ManagedObject;
class ManagedMethod;
struct ObjectReference;
} // namespace dotnet

enum class ObjectFlags : uint32;
enum class ScriptLanguage : uint32;

struct ScriptObjectData_DotNet;
struct ScriptObjectData_HypScript;

#ifdef HYP_SCRIPT
struct Script_Instance;
#endif

HYP_ENUM()
enum class ScriptCompileStatus : uint32
{
    Uninitialized = 0x0,
    Compiled = 0x1,
    Dirty = 0x2,
    Processing = 0x4,
    Errored = 0x8
};

HYP_MAKE_ENUM_FLAGS(ScriptCompileStatus)

HYP_ENUM()
enum class ScriptLanguage : uint32
{
    Invalid = ~0u,

    Native = 0,

    HypScript = 1,
    CSharp = 2
};

} // namespace Hyperion
