/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <generator/generators/BindingLayouts.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Module.hpp>

#include <parser/Parser.hpp>

#include <Core/Reflection/Class.hpp>

#include <cstdlib>
#include <string>

namespace Hyperion {
namespace CodeGen {

namespace {

struct ScalarInfo
{
    const char* cxxName;
    uint32 size;
    const char* rustType;
    const char* cType;
};

const ScalarInfo g_scalars[] = {
    { "bool", 1, "bool", "bool" },
    { "float", 4, "f32", "float" },
    { "double", 8, "f64", "double" },
    { "int8", 1, "i8", "int8_t" },
    { "uint8", 1, "u8", "uint8_t" },
    { "ubyte", 1, "u8", "uint8_t" },
    { "int16", 2, "i16", "int16_t" },
    { "uint16", 2, "u16", "uint16_t" },
    { "int32", 4, "i32", "int32_t" },
    { "uint32", 4, "u32", "uint32_t" },
    { "int", 4, "i32", "int32_t" },
    { "uint", 4, "u32", "uint32_t" },
    { "int64", 8, "i64", "int64_t" },
    { "uint64", 8, "u64", "uint64_t" }
};

const ScalarInfo* FindScalar(const String& cxxName)
{
    for (const ScalarInfo& scalar : g_scalars)
    {
        if (cxxName == scalar.cxxName)
        {
            return &scalar;
        }
    }

    return nullptr;
}

struct BuiltinInfo
{
    const char* name;
    uint32 size;
    uint32 align;
};

// Defined by hand in hyperion-sys and the C header prelude (the math templates aren't reflected structs).
const BuiltinInfo g_builtins[] = {
    { "Vec2f", 8, 8 }, { "Vec3f", 16, 16 }, { "Vec4f", 16, 16 },
    { "Vec2i", 8, 8 }, { "Vec2u", 8, 8 },
    { "Name", 8, 8 }, { "StringHash", 8, 8 }, { "Color", 4, 4 }
};

struct DescribedField
{
    const char* name;
    const char* cxxTypeName;
    uint32 count;
};

struct DescribedInfo
{
    const char* name;
    uint32 align;
    DescribedField fields[2];
};

// Reflected structs whose storage isn't a list of HYP_FIELDs.
const DescribedInfo g_described[] = {
    { "Mat4f", 16, { { "values", "float", 16 }, { nullptr, nullptr, 0 } } },
    { "Mat3f", 16, { { "rows", "Vec3f", 3 }, { nullptr, nullptr, 0 } } }
};

uint32 AlignUp(uint32 value, uint32 align)
{
    return (value + align - 1) / align * align;
}

// `alignas(16)` in the struct's declaration; 0 when there is none or it isn't a plain number.
uint32 FindDeclaredAlignment(const ClassDefinition& cls)
{
    const std::string source = cls.source.Data();
    const size_t index = source.find("alignas(");

    if (index == std::string::npos)
    {
        return 0;
    }

    const char* begin = source.c_str() + index + 8;
    char* end = nullptr;
    const long value = std::strtol(begin, &end, 10);

    while (*end == ' ')
    {
        ++end;
    }

    // alignas(alignof(T) * 2) and the like can't be evaluated here
    return (end != begin && *end == ')' && value > 0) ? uint32(value) : ~0u;
}

} // anonymous namespace

BindingLayoutSet::BindingLayoutSet(const Analyzer& analyzer)
    : m_analyzer(analyzer)
{
    for (const BuiltinInfo& builtin : g_builtins)
    {
        BindingLayout layout;
        layout.name = builtin.name;
        layout.size = builtin.size;
        layout.align = builtin.align;
        layout.isBuiltin = true;

        m_layouts.Set(layout.name, std::move(layout));
    }
}

bool BindingLayoutSet::IsGenerated(const String& typeName) const
{
    const auto it = m_layouts.Find(typeName);

    return it != m_layouts.End() && !it->second.isBuiltin;
}

Array<const BindingLayout*> BindingLayoutSet::GetGeneratedLayouts() const
{
    Array<const BindingLayout*> layouts;

    for (const String& name : m_order)
    {
        layouts.PushBack(&m_layouts.At(name));
    }

    return layouts;
}

const BindingLayout* BindingLayoutSet::Resolve(const String& typeName)
{
    if (const auto it = m_layouts.Find(typeName); it != m_layouts.End())
    {
        return &it->second;
    }

    if (m_unsupported.Contains(typeName) || m_inProgress.Contains(typeName))
    {
        return nullptr;
    }

    m_inProgress.Insert(typeName);

    BindingLayout layout;
    const bool supported = BuildStructLayout(typeName, layout);

    m_inProgress.Erase(typeName);

    if (!supported)
    {
        m_unsupported.Insert(typeName);

        return nullptr;
    }

    m_layouts.Set(typeName, std::move(layout));
    m_order.PushBack(typeName);

    return &m_layouts.At(typeName);
}

bool BindingLayoutSet::ResolveFieldType(const String& cxxTypeName, ResolvedType& out)
{
    if (const ScalarInfo* scalar = FindScalar(cxxTypeName))
    {
        out = { scalar->size, scalar->size, scalar->rustType, scalar->cType };

        return true;
    }

    const ClassDefinition* definition = m_analyzer.FindClassDefinition(cxxTypeName);

    if (definition != nullptr && definition->type == ClassDefinitionType::Enum)
    {
        if (definition->baseClassNames.Size() > 1)
        {
            return false;
        }

        // an enum is its underlying integer on both sides
        const ScalarInfo* underlying = FindScalar(definition->baseClassNames.Any() ? definition->baseClassNames.Front() : String("int"));

        if (underlying == nullptr)
        {
            return false;
        }

        out = { underlying->size, underlying->size, underlying->rustType, underlying->cType };

        return true;
    }

    const BindingLayout* layout = Resolve(cxxTypeName);

    if (layout == nullptr)
    {
        return false;
    }

    out = { layout->size, layout->align, layout->name, "Hyp" + layout->name };

    return true;
}

bool BindingLayoutSet::BuildStructLayout(const String& typeName, BindingLayout& out)
{
    const ClassDefinition* definition = m_analyzer.FindClassDefinition(typeName);

    if (definition == nullptr || definition->type != ClassDefinitionType::Struct || definition->baseClassNames.Any())
    {
        return false;
    }

    // HYP_STRUCT(NoBindingLayout): the struct has members that aren't HYP_FIELDs
    for (const Pair<String, ClassAttributeValue>& attribute : definition->attributes)
    {
        if (attribute.first.ToLower() == "nobindinglayout")
        {
            return false;
        }
    }

    out.name = typeName;

    struct PendingField
    {
        String name;
        String cxxTypeName;
        uint32 count;
    };

    Array<PendingField> pendingFields;
    uint32 declaredAlignment = FindDeclaredAlignment(*definition);

    if (declaredAlignment == ~0u)
    {
        return false;
    }

    const DescribedInfo* described = nullptr;

    for (const DescribedInfo& candidate : g_described)
    {
        if (typeName == candidate.name)
        {
            described = &candidate;
        }
    }

    if (described != nullptr)
    {
        declaredAlignment = described->align;

        for (const DescribedField& field : described->fields)
        {
            if (field.name != nullptr)
            {
                pendingFields.PushBack({ field.name, field.cxxTypeName, field.count });
            }
        }
    }
    else
    {
        for (const MemberDef& member : definition->members)
        {
            if (member.type != MemberType::Field)
            {
                continue;
            }

            const ASTType* type = member.cxxType.Get();

            if (type == nullptr || type->isStatic)
            {
                return false;
            }

            uint32 count = 0;

            if (type->isArray)
            {
                const ASTLiteralInt* length = dynamic_cast<const ASTLiteralInt*>(type->arrayExpr.Get());

                if (length == nullptr || length->value <= 0 || type->arrayOf == nullptr)
                {
                    return false;
                }

                count = uint32(length->value);
                type = type->arrayOf.Get();
            }

            if (type->isPointer || type->isLvalueReference || type->isRvalueReference || type->isTemplate || type->isArray
                || type->isFunctionPointer || !type->typeName.HasValue() || type->typeName->parts.Empty())
            {
                return false;
            }

            pendingFields.PushBack({ member.name, type->typeName->ToString(/* includeNamespace */ false), count });
        }
    }

    if (pendingFields.Empty())
    {
        return false;
    }

    uint32 offset = 0;
    uint32 align = 1;

    for (const PendingField& pending : pendingFields)
    {
        ResolvedType resolved;

        if (!ResolveFieldType(pending.cxxTypeName, resolved))
        {
            return false;
        }

        offset = AlignUp(offset, resolved.align);

        BindingLayoutField field;
        field.cxxName = pending.name;
        field.name = pending.name.StartsWith("m_") ? String(pending.name.Substr(2, pending.name.Size())) : pending.name;
        field.rustType = resolved.rustType;
        field.cType = resolved.cType;
        field.count = pending.count;
        field.offset = offset;

        out.fields.PushBack(std::move(field));

        offset += resolved.size * (pending.count != 0 ? pending.count : 1);
        align = resolved.align > align ? resolved.align : align;
    }

    if (declaredAlignment > align)
    {
        align = declaredAlignment;
    }

    out.align = align;
    out.size = AlignUp(offset, align);

    // HYP_STRUCT(Size = N) is the engine's own statement of the size: a struct with members that aren't reflected
    // won't add up to it, and has to stay opaque
    if (const ClassAttributeValue& sizeAttribute = definition->GetAttribute(Attributes::g_attrSize); sizeAttribute.IsValid() && sizeAttribute.IsInt())
    {
        if (uint32(sizeAttribute.GetInt()) != out.size)
        {
            return false;
        }
    }

    return true;
}

} // namespace CodeGen
} // namespace Hyperion
