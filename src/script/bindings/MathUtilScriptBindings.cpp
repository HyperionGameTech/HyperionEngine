#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/VMArray.hpp>
#include <script/vm/Exception.hpp>

#include <math/MathUtil.hpp>

namespace hyperion {
namespace script {
namespace bindings {

using namespace vm;

static struct MathUtilScriptBindings : ScriptBindingsBase
{
    MathUtilScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<MathUtil>())
    {
    }

    virtual void Generate(scriptapi2::Context &context) override
    {   
        context.Global("NAN", "float", Value { Value::F32, { .f = MathUtil::NaN<float>() } });
        context.Global("sqrt", "function< float, float >", CxxFn< float, float, &MathUtil::Sqrt<float> >);
        context.Global("pow", "function< float, float, float >", CxxFn< float, float, float, &MathUtil::Pow<float> >);
        context.Global("sin", "function< float, float >", CxxFn< float, float, &MathUtil::Sin >);
        context.Global("cos", "function< float, float >", CxxFn< float, float, &MathUtil::Cos >);
        context.Global("tan", "function< float, float >", CxxFn< float, float, &MathUtil::Tan >);
        context.Global("asin", "function< float, float >", CxxFn< float, float, &MathUtil::Arcsin >);
        context.Global("acos", "function< float, float >", CxxFn< float, float, &MathUtil::Arccos >);
        context.Global("atan", "function< float, float >", CxxFn< float, float, &MathUtil::Arctan >);
    }

} math_util_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion