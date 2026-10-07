/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/CHeaderGenerator.hpp>
#include <generator/generators/BindingsShared.hpp>
#include <generator/generators/BindingLayouts.hpp>

#include <string>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <Core/Containers/Map.hpp>

#include <algorithm>

namespace Hyperion {
namespace CodeGen {

using namespace BindingsShared;

namespace {

// Value types the header defines with the engine's layout; everything else is an opaque struct.
const char* const g_cKnownStructTypes[] = { "Vec2f", "Vec3f", "Vec4f", "Vec2i", "Vec2u", "Name", "StringHash", "Color" };

const BindingLayoutSet* g_cLayouts = nullptr;

bool IsCKnownStructType(const String& typeName)
{
    if (g_cLayouts != nullptr && g_cLayouts->IsGenerated(typeName))
    {
        return true;
    }

    for (const char* knownType : g_cKnownStructTypes)
    {
        if (typeName == knownType)
        {
            return true;
        }
    }

    return false;
}

String CScalarType(const String& typeName)
{
    static const Map<String, String> s_scalars {
        { "bool", "bool" }, { "float", "float" }, { "double", "double" },
        { "int", "int32_t" }, { "uint", "uint32_t" }, { "sbyte", "int8_t" }, { "byte", "uint8_t" },
        { "short", "int16_t" }, { "ushort", "uint16_t" }, { "long", "int64_t" }, { "ulong", "uint64_t" }
    };

    const auto it = s_scalars.Find(typeName);

    return it != s_scalars.End() ? it->second : String::empty;
}

// Engine type names are common words (Node, Name, Color); prefix them so they can't collide with the includer's.
String CTypeName(const String& typeName)
{
    return "Hyp" + typeName;
}

String CValueType(const ThunkBindingValue& value, const Set<String>& enumNames)
{
    const String& convention = value.convention;

    if (convention == "void")
    {
        return "void";
    }

    if (convention == "cstring")
    {
        return "const char*";
    }

    if (convention == "handle" || convention == "handle_retained" || convention == "struct_ptr" || convention == "struct_out")
    {
        return CTypeName(value.type) + "*";
    }

    if (convention == "string_out" || convention == "array_ptr" || convention == "array_out")
    {
        return "HypArray*";
    }

    if (convention == "result")
    {
        return "bool";
    }

    if (convention == "value")
    {
        return enumNames.Contains(value.type) ? CTypeName(value.type) : CScalarType(value.type);
    }

    return String::empty;
}

} // anonymous namespace

String CHeaderGenerator::Format(const Analyzer& analyzer, Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts)
{
    g_cLayouts = &layouts;

    std::sort(records.Begin(), records.End(), [](const ThunkBindingRecord& a, const ThunkBindingRecord& b)
        {
            return a.symbol < b.symbol;
        });

    const Array<BindingEnum> enums = CollectBindingEnums(analyzer);

    Set<String> enumNames;

    for (const BindingEnum& bindingEnum : enums)
    {
        enumNames.Insert(bindingEnum.name);
    }

    struct CFunction
    {
        String symbol;
        String returnType;
        String params;
    };

    Array<CFunction> functions;
    Set<String> opaqueTypes;

    for (const ThunkBindingRecord& record : records)
    {
        Array<String> paramTypes;
        Array<String> pointerTypes;
        bool supported = true;

        if (!record.isStatic)
        {
            paramTypes.PushBack(CTypeName(record.className) + "* self");
            pointerTypes.PushBack(record.className);
        }

        auto addValue = [&](const ThunkBindingValue& value) -> String
        {
            const String cType = CValueType(value, enumNames);

            if (cType.Empty())
            {
                supported = false;
            }
            else if (value.convention == "handle" || value.convention == "handle_retained"
                || value.convention == "struct_ptr" || value.convention == "struct_out")
            {
                pointerTypes.PushBack(value.type);
            }

            return cType;
        };

        for (const ThunkBindingValue& param : record.params)
        {
            paramTypes.PushBack(addValue(param) + " " + param.name);
        }

        const String& returnConvention = record.returnValue.convention;
        const String returnType = addValue(record.returnValue);

        const bool returnsThroughParam = returnConvention == "struct_out" || returnConvention == "string_out" || returnConvention == "array_out";

        if (returnsThroughParam)
        {
            paramTypes.PushBack(returnType + " outReturn");
        }
        else if (returnConvention == "result")
        {
            if (record.hasResultValue)
            {
                const String& valueConvention = record.resultValue.convention;
                String valueType = addValue(record.resultValue);

                if (valueConvention == "handle_retained" || valueConvention == "value")
                {
                    valueType += "*";
                }

                paramTypes.PushBack(valueType + " outValue");
            }

            paramTypes.PushBack("HypArray* outError");
        }

        if (!supported)
        {
            continue;
        }

        for (const String& pointerType : pointerTypes)
        {
            if (!IsCKnownStructType(pointerType))
            {
                opaqueTypes.Insert(pointerType);
            }
        }

        functions.PushBack({ record.symbol, returnsThroughParam ? String("void") : returnType, paramTypes.Any() ? String::Join(paramTypes, ", ") : String("void") });
    }

    String result = "/* Auto-generated by Hyperion CodeGen from the engine's reflected classes. Do not edit. */\n\n";
    result += "#ifndef HYPERION_BINDINGS_H\n#define HYPERION_BINDINGS_H\n\n";
    result += "#include <stdbool.h>\n#include <stdint.h>\n\n";
    result += "#ifdef __cplusplus\nextern \"C\" {\n#define HYP_BINDING_ALIGNAS(n) alignas(n)\n#else\n#define HYP_BINDING_ALIGNAS(n) _Alignas(n)\n#endif\n\n";
    result += "#define HYP_BINDING_ABI_VERSION 1\n\n";
    result += "/* A string or array the engine returns through an out-parameter; free `data` with Hyp_Free. */\n";
    result += "typedef struct HypArray { void* data; uint32_t length; uint32_t cap; } HypArray;\n\n";
    result += "/* Value types, laid out as the engine's. */\n";
    result += "typedef struct HypVec2f { HYP_BINDING_ALIGNAS(8) float x; float y; } HypVec2f;\n";
    result += "typedef struct HypVec3f { HYP_BINDING_ALIGNAS(16) float x; float y; float z; } HypVec3f;\n";
    result += "typedef struct HypVec4f { HYP_BINDING_ALIGNAS(16) float x; float y; float z; float w; } HypVec4f;\n";
    result += "typedef struct HypVec2i { HYP_BINDING_ALIGNAS(8) int32_t x; int32_t y; } HypVec2i;\n";
    result += "typedef struct HypVec2u { HYP_BINDING_ALIGNAS(8) uint32_t x; uint32_t y; } HypVec2u;\n";
    result += "typedef struct HypName { uint64_t hash; } HypName;\n";
    result += "typedef struct HypStringHash { uint64_t hash; } HypStringHash;\n";
    result += "typedef struct HypColor { uint32_t value; } HypColor;\n\n";

    result += "/* Structs with the engine's layout (the engine build asserts each one). */\n";

    for (const BindingLayout* layout : layouts.GetGeneratedLayouts())
    {
        result += "typedef struct " + CTypeName(layout->name) + " {";

        for (size_t i = 0; i < layout->fields.Size(); ++i)
        {
            const BindingLayoutField& field = layout->fields[i];

            result += " ";

            // the struct's alignment goes on its first member
            if (i == 0)
            {
                result += "HYP_BINDING_ALIGNAS(" + String(std::to_string(layout->align).c_str()) + ") ";
            }

            result += field.cType + " " + field.name;

            if (field.count != 0)
            {
                result += "[" + String(std::to_string(field.count).c_str()) + "]";
            }

            result += ";";
        }

        result += " } " + CTypeName(layout->name) + ";\n";
    }

    result += "\n";

    Array<String> sortedOpaqueTypes;

    for (const String& opaqueType : opaqueTypes)
    {
        sortedOpaqueTypes.PushBack(opaqueType);
    }

    std::sort(sortedOpaqueTypes.Begin(), sortedOpaqueTypes.End());

    result += "/* Engine objects and structs that are only ever held by pointer. */\n";

    for (const String& opaqueType : sortedOpaqueTypes)
    {
        result += "typedef struct " + CTypeName(opaqueType) + " " + CTypeName(opaqueType) + ";\n";
    }

    result += "\n";

    for (const BindingEnum& bindingEnum : enums)
    {
        const String underlying = CScalarType(bindingEnum.underlying);

        if (underlying.Empty())
        {
            continue;
        }

        result += "typedef " + underlying + " " + CTypeName(bindingEnum.name) + ";\n";

        for (const Pair<String, String>& constant : bindingEnum.constants)
        {
            result += "#define " + CTypeName(bindingEnum.name) + "_" + constant.first + " ((" + CTypeName(bindingEnum.name) + ")" + constant.second + ")\n";
        }
    }

    result += "\n/* Every binding the engine exports. One this engine build doesn't have is left null by HypBindings_Load.\n";
    result += "   A returned object is borrowed unless Bindings.json marks the return \"handle_retained\" (release with Hyp_Release). */\n";
    result += "typedef struct HypBindings\n{\n";

    for (const CFunction& function : functions)
    {
        result += "    " + function.returnType + " (*" + function.symbol + ")(" + function.params + ");\n";
    }

    result += "} HypBindings;\n\n";
    result += "/* `resolve` is the engine's Hyp_ResolveBinding. */\n";
    result += "static inline void HypBindings_Load(HypBindings* bindings, void* (*resolve)(const char* name))\n{\n";

    for (const CFunction& function : functions)
    {
        result += "    bindings->" + function.symbol + " = (" + function.returnType + " (*)(" + function.params + "))resolve(\"" + function.symbol + "\");\n";
    }

    result += "}\n\n#ifdef __cplusplus\n} /* extern \"C\" */\n#endif\n\n#endif /* HYPERION_BINDINGS_H */\n";

    return result;
}

} // namespace CodeGen
} // namespace Hyperion
