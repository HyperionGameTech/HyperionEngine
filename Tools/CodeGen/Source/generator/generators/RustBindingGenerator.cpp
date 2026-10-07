/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/RustBindingGenerator.hpp>
#include <generator/generators/BindingsShared.hpp>
#include <generator/generators/BindingLayouts.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Core/Containers/Map.hpp>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace Hyperion {
namespace CodeGen {

using namespace BindingsShared;

namespace {

// Value types hyperion-sys defines by hand with a matching layout; everything else is opaque to Rust.
const char* const g_rustKnownStructTypes[] = { "Vec2f", "Vec3f", "Vec4f", "Vec2i", "Vec2u", "Name", "StringHash", "Color" };

// Set while formatting: structs with a generated layout count as known too.
const BindingLayoutSet* g_rustLayouts = nullptr;

bool IsRustKnownStructType(const String& typeName)
{
    if (g_rustLayouts != nullptr && g_rustLayouts->IsGenerated(typeName))
    {
        return true;
    }

    for (const char* knownType : g_rustKnownStructTypes)
    {
        if (typeName == knownType)
        {
            return true;
        }
    }

    return false;
}

// Strata scalar names, as the binding records spell them.
String RustScalarType(const String& typeName)
{
    static const Map<String, String> s_scalars {
        { "bool", "bool" }, { "float", "f32" }, { "double", "f64" },
        { "int", "i32" }, { "uint", "u32" }, { "sbyte", "i8" }, { "byte", "u8" },
        { "short", "i16" }, { "ushort", "u16" }, { "long", "i64" }, { "ulong", "u64" }
    };

    const auto it = s_scalars.Find(typeName);

    return it != s_scalars.End() ? it->second : String::empty;
}

// The Rust FFI type of one value, or empty when it can't be expressed.
String RustValueType(const ThunkBindingValue& value, const Set<String>& enumNames, bool isReturn)
{
    const String& convention = value.convention;

    if (convention == "void")
    {
        return "()";
    }

    if (convention == "cstring")
    {
        return "*const c_char";
    }

    if (convention == "handle" || convention == "handle_retained")
    {
        return "*mut " + value.type;
    }

    if (convention == "struct_ptr" || convention == "struct_out")
    {
        return "*mut " + value.type;
    }

    if (convention == "string_out")
    {
        return "*mut HypString";
    }

    if (convention == "result")
    {
        return "bool";
    }

    if (convention == "array_ptr" || convention == "array_out")
    {
        // "int[]"
        if (!value.type.EndsWith("[]"))
        {
            return String::empty;
        }

        const String elementType = RustScalarType(String(value.type.Substr(0, value.type.Size() - 2)));

        return elementType.Any() ? "*mut HypArray<" + elementType + ">" : String::empty;
    }

    if (convention == "value")
    {
        if (enumNames.Contains(value.type))
        {
            return value.type;
        }

        return RustScalarType(value.type);
    }

    return String::empty;
}

// GetUIStage -> get_ui_stage, SetFOV -> set_fov
String RustSnakeCase(const String& name)
{
    std::string result;
    const char* chars = name.Data();
    const size_t length = name.Size();

    auto isUpper = [](char ch) { return ch >= 'A' && ch <= 'Z'; };
    auto isLower = [](char ch) { return ch >= 'a' && ch <= 'z'; };
    auto isDigit = [](char ch) { return ch >= '0' && ch <= '9'; };

    for (size_t i = 0; i < length; ++i)
    {
        const char ch = chars[i];

        if (ch == '_')
        {
            if (!result.empty() && result.back() != '_')
            {
                result.push_back('_');
            }

            continue;
        }

        if (isUpper(ch) && !result.empty() && result.back() != '_')
        {
            const char previous = chars[i - 1];
            const char next = i + 1 < length ? chars[i + 1] : '\0';

            // a new word starts after a lowercase letter or digit, or where an acronym ends
            if (isLower(previous) || isDigit(previous) || (isUpper(previous) && isLower(next)))
            {
                result.push_back('_');
            }
        }

        result.push_back(isUpper(ch) ? char(ch - 'A' + 'a') : ch);
    }

    return String(result.c_str());
}

String RustIdentifier(const String& name)
{
    static const char* const s_keywords[] = {
        "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum", "extern", "false", "fn",
        "for", "if", "impl", "in", "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return", "self", "static",
        "struct", "super", "trait", "true", "type", "unsafe", "use", "where", "while", "abstract", "become", "box", "do",
        "final", "gen", "macro", "override", "priv", "try", "typeof", "unsized", "virtual", "yield"
    };

    String identifier = RustSnakeCase(name);

    if (identifier.Empty())
    {
        return "value";
    }

    for (const char* keyword : s_keywords)
    {
        if (identifier == keyword)
        {
            return identifier + "_";
        }
    }

    return identifier;
}

// Nearest base class that also has a wrapper, so a derived object can be used where its base is expected.
String FindRustWrapperBase(const Analyzer& analyzer, const ClassDefinition& cls, const Set<String>& wrappedClasses, int depth = 0)
{
    if (depth > 32)
    {
        return String::empty;
    }

    for (const String& baseName : cls.baseClassNames)
    {
        if (wrappedClasses.Contains(baseName))
        {
            return baseName;
        }
    }

    for (const String& baseName : cls.baseClassNames)
    {
        if (const ClassDefinition* base = analyzer.FindClassDefinition(baseName))
        {
            const String found = FindRustWrapperBase(analyzer, *base, wrappedClasses, depth + 1);

            if (found.Any())
            {
                return found;
            }
        }
    }

    return String::empty;
}

struct RustWrapperMethod
{
    String className;
    String text;
};

// One safe method over a raw binding, or empty when a parameter or the return can't be expressed safely
// (a struct Rust can't construct, an object type without a wrapper).
String FormatRustWrapperMethod(const ThunkBindingRecord& record, const String& methodName, const Set<String>& wrappedClasses, const Set<String>& enumNames)
{
    Array<String> params;
    Array<String> prelude;
    Array<String> arguments;

    if (!record.isStatic)
    {
        params.PushBack("&self");
        arguments.PushBack("self.as_ptr()");
    }

    Set<String> usedNames;

    for (const ThunkBindingValue& param : record.params)
    {
        String name = RustIdentifier(param.name);

        while (usedNames.Contains(name) || name == "out" || name == "error" || name == "value" || name == "binding")
        {
            name += "_";
        }

        usedNames.Insert(name);

        const String& convention = param.convention;

        if (convention == "value")
        {
            const String type = enumNames.Contains(param.type) ? "sys::" + param.type : RustScalarType(param.type);

            if (type.Empty())
            {
                return String::empty;
            }

            params.PushBack(name + ": " + type);
            arguments.PushBack(name);
        }
        else if (convention == "handle")
        {
            if (!wrappedClasses.Contains(param.type))
            {
                return String::empty;
            }

            params.PushBack(name + ": &" + param.type);
            arguments.PushBack(name + ".as_ptr()");
        }
        else if (convention == "struct_ptr")
        {
            if (!IsRustKnownStructType(param.type))
            {
                return String::empty;
            }

            params.PushBack(name + ": sys::" + param.type);
            prelude.PushBack("let mut " + name + " = " + name + ";");
            arguments.PushBack("&mut " + name);
        }
        else if (convention == "cstring")
        {
            params.PushBack(name + ": &str");
            prelude.PushBack("let " + name + " = to_c_string(" + name + ");");
            arguments.PushBack(name + ".as_ptr()");
        }
        else if (convention == "array_ptr")
        {
            const String elementType = param.type.EndsWith("[]") ? RustScalarType(String(param.type.Substr(0, param.type.Size() - 2))) : String::empty;

            if (elementType.Empty())
            {
                return String::empty;
            }

            params.PushBack(name + ": &[" + elementType + "]");
            prelude.PushBack("let mut " + name + " = sys::HypArray { data: " + name + ".as_ptr() as *mut " + elementType + ", length: " + name + ".len() as u32, cap: " + name + ".len() as u32 };");
            arguments.PushBack("&mut " + name);
        }
        else
        {
            return String::empty;
        }
    }

    const String& returnConvention = record.returnValue.convention;
    const String& returnTypeName = record.returnValue.type;

    String returnType;
    String body;

    const String call = "binding(" + String::Join(arguments, ", ");

    if (returnConvention == "void")
    {
        body = "unsafe { " + call + ") }";
    }
    else if (returnConvention == "value")
    {
        returnType = enumNames.Contains(returnTypeName) ? "sys::" + returnTypeName : RustScalarType(returnTypeName);

        if (returnType.Empty())
        {
            return String::empty;
        }

        body = "unsafe { " + call + ") }";
    }
    else if (returnConvention == "handle" || returnConvention == "handle_retained")
    {
        if (!wrappedClasses.Contains(returnTypeName))
        {
            return String::empty;
        }

        if (returnConvention == "handle")
        {
            returnType = "Option<" + returnTypeName + ">";
            body = "unsafe { " + returnTypeName + "::from_ptr(" + call + ")) }";
        }
        else
        {
            returnType = "Option<Owned<" + returnTypeName + ">>";
            body = "unsafe { Owned::from_retained(" + call + ")) }";
        }
    }
    else if (returnConvention == "struct_out")
    {
        if (!IsRustKnownStructType(returnTypeName))
        {
            return String::empty;
        }

        returnType = "sys::" + returnTypeName;
        prelude.PushBack("let mut out = sys::" + returnTypeName + "::default();");
        body = "unsafe { " + call + (arguments.Any() ? ", " : "") + "&mut out) };\n        out";
    }
    else if (returnConvention == "result" && record.hasResultValue)
    {
        const String& valueConvention = record.resultValue.convention;
        const String& valueTypeName = record.resultValue.type;

        String okType;
        String okExpression;

        if (valueConvention == "handle_retained")
        {
            if (!wrappedClasses.Contains(valueTypeName))
            {
                return String::empty;
            }

            okType = "Option<Owned<" + valueTypeName + ">>";
            prelude.PushBack("let mut value: *mut sys::" + valueTypeName + " = core::ptr::null_mut();");
            okExpression = "unsafe { Owned::from_retained(value) }";
        }
        else if (valueConvention == "string_out")
        {
            okType = "String";
            prelude.PushBack("let mut value = sys::HypString::empty();");
            okExpression = "unsafe { take_string(value) }";
        }
        else if (valueConvention == "struct_out")
        {
            if (!IsRustKnownStructType(valueTypeName))
            {
                return String::empty;
            }

            okType = "sys::" + valueTypeName;
            prelude.PushBack("let mut value = sys::" + valueTypeName + "::default();");
            okExpression = "value";
        }
        else if (valueConvention == "value")
        {
            okType = enumNames.Contains(valueTypeName) ? "sys::" + valueTypeName : RustScalarType(valueTypeName);

            if (okType.Empty())
            {
                return String::empty;
            }

            prelude.PushBack("let mut value: " + okType + " = Default::default();");
            okExpression = "value";
        }
        else
        {
            return String::empty;
        }

        returnType = "Result<" + okType + ", String>";
        prelude.PushBack("let mut error = sys::HypString::empty();");
        body = "if unsafe { " + call + (arguments.Any() ? ", " : "") + "&mut value, &mut error) } {\n            Ok(" + okExpression
            + ")\n        } else {\n            Err(unsafe { take_string(error) })\n        }";
    }
    else if (returnConvention == "result")
    {
        returnType = "Result<(), String>";
        prelude.PushBack("let mut error = sys::HypString::empty();");
        body = "if unsafe { " + call + (arguments.Any() ? ", " : "") + "&mut error) } {\n            Ok(())\n        } else {\n            Err(unsafe { take_string(error) })\n        }";
    }
    else if (returnConvention == "string_out")
    {
        returnType = "String";
        prelude.PushBack("let mut out = sys::HypString::empty();");
        body = "unsafe {\n            " + call + (arguments.Any() ? ", " : "") + "&mut out);\n            take_string(out)\n        }";
    }
    else if (returnConvention == "array_out")
    {
        const String elementType = returnTypeName.EndsWith("[]") ? RustScalarType(String(returnTypeName.Substr(0, returnTypeName.Size() - 2))) : String::empty;

        if (elementType.Empty())
        {
            return String::empty;
        }

        returnType = "Vec<" + elementType + ">";
        prelude.PushBack("let mut out = sys::HypArray::<" + elementType + ">::empty();");
        body = "unsafe {\n            " + call + (arguments.Any() ? ", " : "") + "&mut out);\n            take_array(out)\n        }";
    }
    else
    {
        return String::empty;
    }

    String text = "    /// `" + record.className + "::" + record.memberName + "`" + (record.kind == "method" ? "" : " (property)");

    if (record.condition.Any())
    {
        text += ". Only in engine builds with `" + record.condition + "`; panics otherwise";
    }

    text += "\n    pub fn " + methodName + "(" + String::Join(params, ", ") + ")";

    if (returnType.Any())
    {
        text += " -> " + returnType;
    }

    text += " {\n        let binding = sys::bindings()." + record.symbol + ".expect(\"engine binding " + record.symbol + " is not available\");\n";

    for (const String& line : prelude)
    {
        text += "        " + line + "\n";
    }

    text += "        " + body + "\n    }\n";

    return text;
}

} // anonymous namespace

String RustBindingGenerator::Format(const Analyzer& analyzer, Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts)
{
    g_rustLayouts = &layouts;

    std::sort(records.Begin(), records.End(), [](const ThunkBindingRecord& a, const ThunkBindingRecord& b)
        {
            return a.symbol < b.symbol;
        });

    const Array<BindingEnum> enums = CollectBindingEnums(analyzer);

    Set<String> enumNames;

    for (const BindingEnum& rustEnum : enums)
    {
        enumNames.Insert(rustEnum.name);
    }

    struct RustFunction
    {
        String symbol;
        String signature;
    };

    Array<RustFunction> functions;
    Array<String> skipped;
    Set<String> opaqueTypes;

    for (const ThunkBindingRecord& record : records)
    {
        Array<String> paramTypes;
        Array<String> pointerTypes;
        bool supported = true;

        if (!record.isStatic)
        {
            paramTypes.PushBack("*mut " + record.className);
            pointerTypes.PushBack(record.className);
        }

        auto addValue = [&](const ThunkBindingValue& value, bool isReturn) -> String
        {
            const String rustType = RustValueType(value, enumNames, isReturn);

            if (rustType.Empty())
            {
                supported = false;
            }
            else if (value.convention == "handle" || value.convention == "handle_retained"
                || value.convention == "struct_ptr" || value.convention == "struct_out")
            {
                pointerTypes.PushBack(value.type);
            }

            return rustType;
        };

        for (const ThunkBindingValue& param : record.params)
        {
            paramTypes.PushBack(addValue(param, false));
        }

        const String& returnConvention = record.returnValue.convention;
        const String returnType = addValue(record.returnValue, true);

        // out-parameter returns are the trailing argument of a void function
        const bool returnsThroughParam = returnConvention == "struct_out" || returnConvention == "string_out" || returnConvention == "array_out";

        if (returnsThroughParam)
        {
            paramTypes.PushBack(returnType);
        }
        else if (returnConvention == "result")
        {
            // the value, written only on success
            if (record.hasResultValue)
            {
                const String& valueConvention = record.resultValue.convention;
                String valueType = addValue(record.resultValue, true);

                // an object comes back as a pointer to fill in; a plain value needs a pointer to its storage
                if (valueConvention == "handle_retained" || valueConvention == "value")
                {
                    valueType = "*mut " + valueType;
                }

                paramTypes.PushBack(valueType);
            }

            // the error message, written only on failure
            paramTypes.PushBack("*mut HypString");
        }

        if (!supported)
        {
            skipped.PushBack(record.symbol);

            continue;
        }

        for (const String& pointerType : pointerTypes)
        {
            if (!IsRustKnownStructType(pointerType))
            {
                opaqueTypes.Insert(pointerType);
            }
        }

        String signature = "unsafe extern \"C\" fn(" + String::Join(paramTypes, ", ") + ")";

        if (!returnsThroughParam && returnConvention != "void")
        {
            signature += " -> " + returnType;
        }

        functions.PushBack({ record.symbol, std::move(signature) });
    }

    String result = "// Auto-generated by Hyperion CodeGen from the engine's reflected classes. Do not edit.\n";
    result += "// Included by the hyperion-sys crate, which defines HypString, HypArray and the value types.\n\n";
    result += "pub const BINDING_ABI_VERSION: i32 = 1;\n\n";

    Array<String> sortedOpaqueTypes;

    for (const String& opaqueType : opaqueTypes)
    {
        sortedOpaqueTypes.PushBack(opaqueType);
    }

    std::sort(sortedOpaqueTypes.Begin(), sortedOpaqueTypes.End());

    result += "// Engine objects and structs Rust only ever holds by pointer.\n";

    for (const String& opaqueType : sortedOpaqueTypes)
    {
        result += "#[repr(C)]\npub struct " + opaqueType + " {\n    _opaque: [u8; 0],\n}\n";
    }

    result += "\n// Structs with the engine's layout (the engine build asserts each one).\n";

    for (const BindingLayout* layout : layouts.GetGeneratedLayouts())
    {
        result += "#[repr(C, align(" + String(std::to_string(layout->align).c_str()) + "))]\n#[derive(Clone, Copy, Debug, Default, PartialEq)]\n";
        result += "pub struct " + layout->name + " {\n";

        for (const BindingLayoutField& field : layout->fields)
        {
            const String fieldType = field.count != 0
                ? "[" + field.rustType + "; " + String(std::to_string(field.count).c_str()) + "]"
                : field.rustType;

            result += "    pub " + RustIdentifier(field.name) + ": " + fieldType + ",\n";
        }

        result += "}\n";
        result += "const _: () = assert!(core::mem::size_of::<" + layout->name + ">() == " + String(std::to_string(layout->size).c_str()) + ");\n";
    }

    result += "\n";

    for (const BindingEnum& rustEnum : enums)
    {
        result += "pub type " + rustEnum.name + " = " + RustScalarType(rustEnum.underlying) + ";\n";

        for (const Pair<String, String>& constant : rustEnum.constants)
        {
            result += "pub const " + rustEnum.name + "_" + constant.first + ": " + rustEnum.name + " = (" + constant.second + "i128) as " + rustEnum.name + ";\n";
        }
    }

    result += "\n/// Every binding the engine exports, resolved by name. A binding this engine build doesn't have is `None`.\n";
    result += "pub struct Bindings {\n";

    for (const RustFunction& function : functions)
    {
        result += "    pub " + function.symbol + ": Option<" + function.signature + ">,\n";
    }

    result += "}\n\n";
    result += "impl Bindings {\n";
    result += "    /// # Safety\n    /// `resolve` must be the engine's `Hyp_ResolveBinding`.\n";
    result += "    pub unsafe fn load(resolve: unsafe extern \"C\" fn(*const c_char) -> *mut c_void) -> Self {\n";
    result += "        unsafe {\n            Self {\n";

    for (const RustFunction& function : functions)
    {
        result += "                " + function.symbol + ": core::mem::transmute::<*mut c_void, Option<" + function.signature + ">>(resolve(c\"" + function.symbol + "\".as_ptr())),\n";
    }

    result += "            }\n        }\n    }\n}\n";

    if (skipped.Any())
    {
        result += "\n// Not expressible yet: " + String::Join(skipped, ", ") + "\n";
    }

    return result;
}

String RustBindingGenerator::FormatWrappers(const Analyzer& analyzer, Array<ThunkBindingRecord> records, const BindingLayoutSet& layouts)
{
    g_rustLayouts = &layouts;

    std::sort(records.Begin(), records.End(), [](const ThunkBindingRecord& a, const ThunkBindingRecord& b)
        {
            return a.symbol < b.symbol;
        });

    Set<String> enumNames;

    for (const BindingEnum& bindingEnum : CollectBindingEnums(analyzer))
    {
        enumNames.Insert(bindingEnum.name);
    }

    // Every reflected class the bindings mention gets a wrapper type, including ones that are only passed around.
    Set<String> wrappedClasses;

    auto addClass = [&](const String& typeName)
    {
        const ClassDefinition* definition = analyzer.FindClassDefinition(typeName);

        if (definition != nullptr && definition->type == ClassDefinitionType::Class)
        {
            wrappedClasses.Insert(typeName);
        }
    };

    for (const ThunkBindingRecord& record : records)
    {
        if (!record.isStatic)
        {
            addClass(record.className);
        }

        for (const ThunkBindingValue& param : record.params)
        {
            if (param.convention == "handle")
            {
                addClass(param.type);
            }
        }

        if (record.returnValue.convention == "handle" || record.returnValue.convention == "handle_retained")
        {
            addClass(record.returnValue.type);
        }

        if (record.hasResultValue && record.resultValue.convention == "handle_retained")
        {
            addClass(record.resultValue.type);
        }
    }

    Map<String, Array<String>> methodsByClass;
    Map<String, Set<String>> methodNamesByClass;
    uint32 numWrapped = 0;
    uint32 numSkipped = 0;

    for (const ThunkBindingRecord& record : records)
    {
        // statics live on their class too; a struct or enum has no wrapper to hang methods on
        if (!wrappedClasses.Contains(record.className))
        {
            ++numSkipped;

            continue;
        }

        String methodName = RustIdentifier(record.memberName);

        if (record.kind == "getter")
        {
            methodName = "get_" + RustSnakeCase(record.memberName);
        }
        else if (record.kind == "setter")
        {
            methodName = "set_" + RustSnakeCase(record.memberName);
        }

        Set<String>& usedNames = methodNamesByClass[record.className];

        // names the EngineObject trait already gives every wrapper
        while (usedNames.Contains(methodName) || methodName == "as_ptr" || methodName == "from_ptr" || methodName == "retain")
        {
            methodName += "_";
        }

        const String methodText = FormatRustWrapperMethod(record, methodName, wrappedClasses, enumNames);

        if (methodText.Empty())
        {
            ++numSkipped;

            continue;
        }

        usedNames.Insert(methodName);
        methodsByClass[record.className].PushBack(methodText);

        ++numWrapped;
    }

    Array<String> sortedClasses;

    for (const String& className : wrappedClasses)
    {
        sortedClasses.PushBack(className);
    }

    std::sort(sortedClasses.Begin(), sortedClasses.End());

    String result = "// Auto-generated by Hyperion CodeGen from the engine's reflected classes. Do not edit.\n";
    result += "// Included by the hyperion crate as its `engine` module: a safe wrapper type per engine class.\n";
    result += String("// ") + std::to_string(numWrapped).c_str() + " methods wrapped; " + std::to_string(numSkipped).c_str()
        + " bindings are only reachable through the raw table (sys::bindings).\n\n";

    for (const String& className : sortedClasses)
    {
        const ClassDefinition* definition = analyzer.FindClassDefinition(className);

        result += "/// A borrowed engine `" + className + "`. It stays valid while the engine keeps the object alive; call\n";
        result += "/// [`EngineObject::retain`] to hold on to it.\n";
        result += "#[repr(transparent)]\n#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]\n";
        result += "pub struct " + className + "(core::ptr::NonNull<sys::" + className + ">);\n\n";

        result += "unsafe impl EngineObject for " + className + " {\n";
        result += "    type Raw = sys::" + className + ";\n\n";
        result += "    fn as_ptr(&self) -> *mut sys::" + className + " {\n        self.0.as_ptr()\n    }\n\n";
        result += "    unsafe fn from_ptr(ptr: *mut sys::" + className + ") -> Option<Self> {\n        core::ptr::NonNull::new(ptr).map(Self)\n    }\n}\n\n";

        const String baseName = definition != nullptr ? FindRustWrapperBase(analyzer, *definition, wrappedClasses) : String::empty;

        if (baseName.Any())
        {
            result += "impl core::ops::Deref for " + className + " {\n";
            result += "    type Target = " + baseName + ";\n\n";
            result += "    fn deref(&self) -> &" + baseName + " {\n";
            result += "        // both are a transparent pointer to the same engine object\n";
            result += "        unsafe { &*(self as *const Self as *const " + baseName + ") }\n    }\n}\n\n";
        }

        const auto methodsIt = methodsByClass.Find(className);

        if (methodsIt != methodsByClass.End() && methodsIt->second.Any())
        {
            result += "impl " + className + " {\n";
            result += String::Join(methodsIt->second, "\n");
            result += "}\n\n";
        }
    }

    return result;
}

} // namespace CodeGen
} // namespace Hyperion
