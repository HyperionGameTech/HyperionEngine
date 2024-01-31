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

struct MathClassStub { };

static struct MathScriptBindings : ScriptBindingsBase
{
    MathScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<MathClassStub>())
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

        context.Class<Vec3f>("Vec3f")
            .StaticMethod("$invoke", "function< Vec3f, any, float, float, float >", CxxFn< Vec3f, const void *, float, float, float,
                [](const void *, float x, float y, float z) -> Vec3f
                {
                    return { x, y, z };
                }
            >)
            .Method("distance", "function< float, Vec3f, Vec3f >", CxxMemberFn< float, Vec3f, const Vec3f &, &Vec3f::Distance >)
            .Method("length", "function< float, Vec3f >", CxxMemberFn< float, Vec3f, &Vec3f::Length >)
            .Method("length_squared", "function< float, Vec3f >", CxxMemberFn< float, Vec3f, &Vec3f::LengthSquared >)
            .Method("normalized", "function< Vec3f, Vec3f >", CxxMemberFn< Vec3f, Vec3f, &Vec3f::Normalized >)
            .Method("normalize", "function< Vec3f, Vec3f >", CxxMemberFn< Vec3f &, Vec3f, &Vec3f::Normalize >)
            .Method("dot", "function< float, Vec3f, Vec3f >", CxxMemberFn< float, Vec3f, const Vec3f &, &Vec3f::Dot >)
            .Method("cross", "function< Vec3f, Vec3f, Vec3f >", CxxMemberFn< Vec3f, Vec3f, const Vec3f &, &Vec3f::Cross >)
            .Method("rotate", "function< Vec3f, Vec3f, Vec3f, float >", CxxMemberFn< Vec3f &, Vec3f, const Vec3f &, float, &Vec3f::Rotate >)
            .Method("lerp", "function< Vec3f, Vec3f, Vec3f, float >", CxxMemberFn< Vec3f &, Vec3f, const Vec3f &, float, &Vec3f::Lerp >)
            .Method("angle_between", "function< float, Vec3f, Vec3f >", CxxMemberFn< float, Vec3f, const Vec3f &, &Vec3f::AngleBetween >)
            .Method("operator*", "function< Vec3f, Vec3f, float >", CxxMemberFn< Vec3f, Vec3f, const Vec3f &, &Vec3f::operator* >)
            .Method("operator*=", "function< Vec3f, Vec3f, Vec3f >", CxxMemberFn< Vec3f &, Vec3f, const Vec3f &, &Vec3f::operator*= >)
            .Method("operator/", "function< Vec3f, Vec3f, float >", CxxMemberFn< Vec3f, Vec3f, const Vec3f &, &Vec3f::operator/ >)
            .Method("operator/=", "function< Vec3f, Vec3f, Vec3f >", CxxMemberFn< Vec3f &, Vec3f, const Vec3f &, &Vec3f::operator/= >)
            .Method("operator+", "function< Vec3f, Vec3f, Vec3f >", CxxMemberFn< Vec3f, Vec3f, const Vec3f &, &Vec3f::operator+ >)
            .Method("operator+=", "function< Vec3f, Vec3f, Vec3f >", CxxMemberFn< Vec3f &, Vec3f, const Vec3f &, &Vec3f::operator+= >)
            .Method("operator-", "function< Vec3f, Vec3f, Vec3f >", CxxMemberFn< Vec3f, Vec3f, const Vec3f &, &Vec3f::operator- >)
            .Method("operator-=", "function< Vec3f, Vec3f, Vec3f >", CxxMemberFn< Vec3f &, Vec3f, const Vec3f &, &Vec3f::operator-= >)
            .Method("get_x", "function< float, Vec3f >", CxxMemberFn< float, Vec3f, &Vec3f::GetX >)
            .Method("set_x", "function< Vec3f, Vec3f, float >", CxxMemberFn< Vec3f &, Vec3f, float, &Vec3f::SetX >)
            .Method("get_y", "function< float, Vec3f >", CxxMemberFn< float, Vec3f, &Vec3f::GetY >)
            .Method("set_y", "function< Vec3f, Vec3f, float >", CxxMemberFn< Vec3f &, Vec3f, float, &Vec3f::SetY >)
            .Method("get_z", "function< float, Vec3f >", CxxMemberFn< float, Vec3f, &Vec3f::GetZ >)
            .Method("set_z", "function< Vec3f, Vec3f, float >", CxxMemberFn< Vec3f &, Vec3f, float, &Vec3f::SetZ >)
            .Build();
    }

} math_util_script_bindings { };

} // namespace bindings
} // namespace script
} // namespace hyperion