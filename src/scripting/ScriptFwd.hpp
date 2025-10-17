#pragma once

#include <core/Types.hpp>
#include <core/Defines.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

namespace dotnet {
class ManagedClass;
class ManagedObject;
class Method;
struct ObjectReference;
} // namespace dotnet

enum class ObjectFlags : uint32;
enum ScriptLanguage : uint32;

struct ScriptObjectData_DotNet;
struct ScriptObjectData_HypScript;

#ifdef HYP_SCRIPT
struct Script_Instance;
#endif

HYP_ENUM()
enum ScriptCompileStatus : uint32
{
    SCS_UNINITIALIZED = 0x0,
    SCS_COMPILED = 0x1,
    SCS_DIRTY = 0x2,
    SCS_PROCESSING = 0x4,
    SCS_ERRORED = 0x8
};

HYP_MAKE_ENUM_FLAGS(ScriptCompileStatus)

HYP_ENUM()
enum ScriptLanguage : uint32
{
    SL_INVALID = ~0u,

    SL_HYPSCRIPT = 0,
    SL_CSHARP = 1
};

} // namespace hyperion