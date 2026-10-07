/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_BINDING_LAYOUTS_HPP
#define HYPERION_CODEGEN_BINDING_LAYOUTS_HPP

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

namespace Hyperion {
namespace CodeGen {

class Analyzer;

struct BindingLayoutField
{
    String name;
    String cxxName;   // member name in the C++ struct, for offsetof
    String rustType;  // element type
    String cType;     // element type
    uint32 count = 0; // array length, 0 when not an array
    uint32 offset = 0;
};

// A struct laid out so that C and Rust can hold it by value: the same size, alignment and field offsets as the
// engine's. The generated engine code asserts each one, so a layout that drifts from the C++ struct fails the build.
struct BindingLayout
{
    String name;
    uint32 size = 0;
    uint32 align = 0;
    Array<BindingLayoutField> fields;

    bool isBuiltin = false; // written by hand in hyperion-sys and the C header prelude, not generated
};

// Works out which structs can be laid out. A struct qualifies when it is a HYP_STRUCT with no base class whose
// reflected fields are scalars, enums, fixed arrays or other structs that qualify.
class BindingLayoutSet
{
public:
    explicit BindingLayoutSet(const Analyzer& analyzer);

    // The layout of a struct, or nullptr when it has to stay opaque.
    const BindingLayout* Resolve(const String& typeName);

    // Generated (non-builtin) layouts resolved so far, each after the structs it contains.
    Array<const BindingLayout*> GetGeneratedLayouts() const;

    bool IsGenerated(const String& typeName) const;

private:
    struct ResolvedType
    {
        uint32 size = 0;
        uint32 align = 0;
        String rustType;
        String cType;
    };

    bool ResolveFieldType(const String& cxxTypeName, ResolvedType& out);
    bool BuildStructLayout(const String& typeName, BindingLayout& out);

    const Analyzer& m_analyzer;

    Map<String, BindingLayout> m_layouts;
    Array<String> m_order;
    Set<String> m_unsupported;
    Set<String> m_inProgress;
};

} // namespace CodeGen
} // namespace Hyperion

#endif
