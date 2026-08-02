#if defined(HYP_TESTS) && defined(HYP_SCRIPT)

#include <Lang/HypScript.hpp>
#include <Lang/SourceFile.hpp>
#include <Lang/Compiler/ErrorList.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Logging/Logger.hpp>

namespace Hyperion {
namespace tests {
namespace script {

namespace {

int g_passCount = 0;
int g_failCount = 0;

void Check(const char* testName, bool condition, const String& detail = "")
{
    if (condition)
    {
        ++g_passCount;
        HYP_LOG(Engine, Info, "[PASS] {}", testName);
    }
    else
    {
        ++g_failCount;
        HYP_LOG(Engine, Error, "[FAIL] {} {}", testName, detail);
    }
}

struct ScriptTest
{
    ScriptInstance* instance = nullptr;

    bool Compile(const String& source)
    {
        SourceFile sourceFile(FilePath("test.hyp"), source.Size());
        sourceFile.ReadIntoBuffer(reinterpret_cast<const ubyte*>(source.Data()), source.Size());

        ErrorList errors;
        instance = HypScript::Compile(sourceFile, errors);

        if (!instance || errors.HasFatalErrors())
        {
            return false;
        }

        return true;
    }

    void Run()
    {
        HypScript::Run(instance);
    }

    BoxedValue GetExported(const char* name)
    {
        BoxedValue val;
        HypScript::GetExportedValue(instance, name, val, false);
        return val;
    }

    BoxedValue GetFunction(const char* name)
    {
        BoxedValue val;
        HypScript::GetFunctionHandle(instance, name, val);
        return val;
    }

    BoxedValue Call(const BoxedValue& func)
    {
        return HypScript::CallFunction(instance, func);
    }

    template <class... Args>
    BoxedValue Call(const BoxedValue& func, Args&&... args)
    {
        return HypScript::CallFunction(instance, func, std::forward<Args>(args)...);
    }

    void Destroy()
    {
        HypScript::DestroyScript(instance);
        instance = nullptr;
    }
};

} // anonymous namespace

HYP_EXPORT void RunScriptTest()
{
    HypScript::Initialize();

    // ── Arithmetic and basic expressions ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export a := 2 + 3
                export b := 10 - 4
                export c := 3 * 7
                export d := 20 / 4
                export e := 17 % 5
                export f := -8
                export g := 2.5 * 4.0
            )"
        ))
        {
            t.Run();
            Check("Script compiles and runs", true);
            Check("Add: 2 + 3 == 5", t.GetExported("a").Is<int64>() && t.GetExported("a").Get<int64>() == 5);
            Check("Sub: 10 - 4 == 6", t.GetExported("b").Is<int64>() && t.GetExported("b").Get<int64>() == 6);
            Check("Mul: 3 * 7 == 21", t.GetExported("c").Is<int64>() && t.GetExported("c").Get<int64>() == 21);
            Check("Div: 20 / 4 == 5", t.GetExported("d").Is<int64>() && t.GetExported("d").Get<int64>() == 5);
            Check("Mod: 17 % 5 == 2", t.GetExported("e").Is<int64>() && t.GetExported("e").Get<int64>() == 2);
            Check("Neg: -8", t.GetExported("f").Is<int64>() && t.GetExported("f").Get<int64>() == -8);
            Check("Float mul: 2.5 * 4.0 == 10.0", t.GetExported("g").Is<double>() && t.GetExported("g").Get<double>() == 10.0);
        }
        else
        {
            Check("Arithmetic script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Boolean and comparison ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export b1 := true
                export b2 := false
                export b3 := !true
                export b4 := 5 == 5
                export b5 := 5 != 3
                export b6 := 5 > 3
                export b7 := 5 >= 5
                export b8 := 3 < 5
                export b9 := 5 <= 5
                export b10 := true && false
                export b11 := true || false
            )"
        ))
        {
            t.Run();
            Check("bool true", t.GetExported("b1").Is<bool>() && t.GetExported("b1").Get<bool>() == true);
            Check("bool false", t.GetExported("b2").Is<bool>() && t.GetExported("b2").Get<bool>() == false);
            Check("!true == false", t.GetExported("b3").Is<bool>() && t.GetExported("b3").Get<bool>() == false);
            Check("5 == 5", t.GetExported("b4").Is<bool>() && t.GetExported("b4").Get<bool>() == true);
            Check("5 != 3", t.GetExported("b5").Is<bool>() && t.GetExported("b5").Get<bool>() == true);
            Check("5 > 3", t.GetExported("b6").Is<bool>() && t.GetExported("b6").Get<bool>() == true);
            Check("5 >= 5", t.GetExported("b7").Is<bool>() && t.GetExported("b7").Get<bool>() == true);
            Check("3 < 5", t.GetExported("b8").Is<bool>() && t.GetExported("b8").Get<bool>() == true);
            Check("5 <= 5", t.GetExported("b9").Is<bool>() && t.GetExported("b9").Get<bool>() == true);
            Check("true && false == false", t.GetExported("b10").Is<bool>() && t.GetExported("b10").Get<bool>() == false);
            Check("true || false == true", t.GetExported("b11").Is<bool>() && t.GetExported("b11").Get<bool>() == true);
        }
        else
        {
            Check("Boolean script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Bitwise operations ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export bw1 := 5 & 3
                export bw2 := 5 | 2
                export bw3 := 5 ^ 3
                export bw4 := ~5
                export bw5 := 1 << 3
                export bw6 := 16 >> 2
            )"
        ))
        {
            t.Run();
            Check("bitwise AND: 5 & 3 == 1", t.GetExported("bw1").Is<int64>() && t.GetExported("bw1").Get<int64>() == 1);
            Check("bitwise OR: 5 | 2 == 7", t.GetExported("bw2").Is<int64>() && t.GetExported("bw2").Get<int64>() == 7);
            Check("bitwise XOR: 5 ^ 3 == 6", t.GetExported("bw3").Is<int64>() && t.GetExported("bw3").Get<int64>() == 6);
            Check("bitwise NOT: ~5", t.GetExported("bw4").Is<int64>());
            Check("left shift: 1 << 3 == 8", t.GetExported("bw5").Is<int64>() && t.GetExported("bw5").Get<int64>() == 8);
            Check("right shift: 16 >> 2 == 4", t.GetExported("bw6").Is<int64>() && t.GetExported("bw6").Get<int64>() == 4);
        }
        else
        {
            Check("Bitwise script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Strings ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export s1 := "hello"
                export s2 := "world"
                export s3 := "hello" + " " + "world"
                export s4 := string(42)
                export s5 := string(true)
                export cmp1 := "abc" == "abc"
                export cmp2 := "abc" != "xyz"
            )"
        ))
        {
            t.Run();
            Check("string literal", t.GetExported("s1").Is<String>() && t.GetExported("s1").Get<String>() == "hello");
            Check("string concat", t.GetExported("s3").Is<String>() && t.GetExported("s3").Get<String>() == "hello world");
            Check("string(42)", t.GetExported("s4").Is<String>() && t.GetExported("s4").Get<String>() == "42");
            Check("string(true)", t.GetExported("s5").Is<String>() && t.GetExported("s5").Get<String>() == "true");
            Check("string compare ==", t.GetExported("cmp1").Is<bool>() && t.GetExported("cmp1").Get<bool>() == true);
            Check("string compare !=", t.GetExported("cmp2").Is<bool>() && t.GetExported("cmp2").Get<bool>() == true);
        }
        else
        {
            Check("String script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── If / else if / else ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                x := 10
                export result : string = ""
                if (x < 5)
                    result = "lt"
                else if (x >= 10)
                    result = "gte"
                else
                    result = "other"
                end
            )"
        ))
        {
            t.Run();
            Check("if/else if hits correct branch", t.GetExported("result").Is<String>() && t.GetExported("result").Get<String>() == "gte");
        }
        else
        {
            Check("If/else script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── For loop ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                sum := 0
                for (i := 0; i < 10; i++)
                    sum += i
                end
                export sum
            )"
        ))
        {
            t.Run();
            Check("for loop sum 0..9 == 45", t.GetExported("sum").Is<int64>() && t.GetExported("sum").Get<int64>() == 45);
        }
        else
        {
            Check("For loop script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── For-in loop ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                ary := [10, 20, 30]
                sum := 0
                for (v in ary)
                    sum += v
                end
                export sum
            )"
        ))
        {
            t.Run();
            Check("for-in sum [10,20,30] == 60", t.GetExported("sum").Is<int64>() && t.GetExported("sum").Get<int64>() == 60);
        }
        else
        {
            Check("For-in script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── While loop ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                i := 0
                while (i < 5)
                    i++
                end
                export i
            )"
        ))
        {
            t.Run();
            Check("while loop counts to 5", t.GetExported("i").Is<int64>() && t.GetExported("i").Get<int64>() == 5);
        }
        else
        {
            Check("While loop script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Functions, closures, default args, named args, varargs ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export add := (a : int, b : int) -> int
                    return a + b
                end

                //export fact := (n : int) -> int
                //    if (n <= 1)
                //        return 1
                //    end
                //    return n * fact(n - 1)
                //end

                export makeClosure := (x : float)
                    return func (y : float)
                        x *= 2.0
                        return x + y
                    end
                end

                export withDefault := (x : int, y : int = 10) -> int
                    return x * y
                end

                export withVarargs := (a : int, y : string, args...) -> int
                    return args.Size()
                end

                export doubleFn := func (x) => x * 2
            )"
        ))
        {
            t.Run();

            BoxedValue addFn = t.GetFunction("add");
            Check("add function found", addFn.IsValid());
            if (addFn.IsValid())
            {
                BoxedValue r = t.Call(addFn, int64(10), int64(20));
                Check("add(10, 20) == 30", r.Is<int64>() && r.Get<int64>() == 30);
            }

            // BoxedValue factFn = t.GetFunction("fact");
            // Check("fact function found", factFn.IsValid());
            // if (factFn.IsValid())
            // {
            //     BoxedValue r = t.Call(factFn, int64(5));
            //     Check("fact(5) == 120", r.Is<int64>() && r.Get<int64>() == 120);
            // }

#if 0
            BoxedValue closureFn = t.GetFunction("makeClosure");
            Check("makeClosure found", closureFn.IsValid());
            if (closureFn.IsValid())
            {
                BoxedValue innerFn = t.Call(closureFn, 3.0);
                Check("closure returned", innerFn.IsValid());
                if (innerFn.IsValid() && innerFn.Is<Handle<ObjectBase>>())
                {
                    const Class* innerFnClass = innerFn.GetTypeInfo()->GetClass();
                    Assert(innerFnClass != nullptr);

                    Method* invokeMethod = innerFnClass->GetMethod("$invoke"_sh);
                    Check("Object has $invoke method", invokeMethod != nullptr);

                    BoxedValue r1 = t.Call(innerFn, 4.0);
                    Check("closure first call: (3*2)+4 == 10", r1.Is<double>() && r1.Get<double>() == 10.0);
                    BoxedValue r2 = t.Call(innerFn, 5.0);
                    Check("closure second call: (6*2)+5 == 17", r2.Is<double>() && r2.Get<double>() == 17.0);
                }
            }
#endif

            BoxedValue defaultFn = t.GetFunction("withDefault");
            Check("withDefault found", defaultFn.IsValid());
            if (defaultFn.IsValid())
            {
                BoxedValue r = t.Call(defaultFn, int64(10));
                Check("withDefault(10) == 100 (default y=10)", r.Is<int64>() && r.Get<int64>() == 100);
            }

            BoxedValue varargsFn = t.GetFunction("withVarargs");
            Check("withVarargs found", varargsFn.IsValid());
            if (varargsFn.IsValid())
            {
                BoxedValue r0 = t.Call(varargsFn, int64(1), String("hi"));
                Check("varargs(1, 'hi') -> 0 extra args", r0.Is<int64>() && r0.Get<int64>() == 0);

                BoxedValue r1 = t.Call(varargsFn, int64(1), String("hi"), int64(99));
                Check("varargs(1, 'hi', 99) -> 1 extra arg", r1.Is<int64>() && r1.Get<int64>() == 1);
            }

            BoxedValue doubleFn = t.GetFunction("doubleFn");
            Check("doubleFn function found", doubleFn.IsValid());
            if (doubleFn.IsValid())
            {
                BoxedValue r = t.Call(doubleFn, int64(21));
                Check("doubleFn(21) == 42 (=> syntax)", r.Is<int64>() && r.Get<int64>() == 42);
            }
        }
        else
        {
            Check("Functions script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Arrays ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export arr1 : Array<int> = [1, 2, 3]
                export emptyArr : Array<int> = []
                export nested : Array<Array<int>> = [[1, 2], [3, 4]]
                export first := arr1[0]
                export last := arr1[-1]
                arr1[1] = 99
                export mutated := arr1[1]
            )"
        ))
        {
            t.Run();
            Check("arr1[0] == 1", t.GetExported("first").Is<int64>() && t.GetExported("first").Get<int64>() == 1);
            Check("arr1[-1] == 3", t.GetExported("last").Is<int64>() && t.GetExported("last").Get<int64>() == 3);
            Check("arr1[1] mutated to 99", t.GetExported("mutated").Is<int64>() && t.GetExported("mutated").Get<int64>() == 99);
        }
        else
        {
            Check("Array script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Type casting and runtime type checking ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export cast1 := 3.14 as int
                export cast2 := 42 as float
                export is1 := 5 is int
                export is2 := "hello" is string
                export is3 := 5 is string
                export ui : uint64 = 1
                export uiIsUint := ui is uint64
                export uiIsInt := ui is int
            )"
        ))
        {
            t.Run();
            Check("3.14 as int == 3", t.GetExported("cast1").Is<int64>() && t.GetExported("cast1").Get<int64>() == 3);
            Check("42 as float == 42.0", t.GetExported("cast2").Is<double>() && t.GetExported("cast2").Get<double>() == 42.0);
            Check("5 is int == true", t.GetExported("is1").Is<bool>() && t.GetExported("is1").Get<bool>() == true);
            Check("'hello' is string == true", t.GetExported("is2").Is<bool>() && t.GetExported("is2").Get<bool>() == true);
            Check("5 is string == false", t.GetExported("is3").Is<bool>() && t.GetExported("is3").Get<bool>() == false);
            Check("uint64 var is uint64", t.GetExported("uiIsUint").Is<bool>() && t.GetExported("uiIsUint").Get<bool>() == true);
        }
        else
        {
            Check("Type cast/is script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Structs ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                struct Point
                    x : float = 0.0
                    y : float = 0.0
                end

                p := new Point
                p.x = 3.5
                p.y = 4.5

                export px := p.x
                export py := p.y
                export ptype := p is Point
            )"
        ))
        {
            t.Run();
            Check("struct Point.x == 3.5", t.GetExported("px").Is<double>() && t.GetExported("px").Get<double>() == 3.5);
            Check("struct Point.y == 4.5", t.GetExported("py").Is<double>() && t.GetExported("py").Get<double>() == 4.5);
            Check("p is Point == true", t.GetExported("ptype").Is<bool>() && t.GetExported("ptype").Get<bool>() == true);
        }
        else
        {
            Check("Struct script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Classes with constructor, methods, static, const ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                class Counter
                    static total : int = 0
                    static const MAX : int = 100
                    val : int = 0

                    Counter(start : int = 0)
                        self.val = start
                        Counter.total = Counter.total + 1
                    end

                    Increment := (amount : int = 1)
                        self.val += amount
                    end
                end

                c1 := new Counter(5)
                c1.Increment()
                c1.Increment(4)
                c2 := new Counter

                export c1Val := c1.val
                export c2Val := c2.val
                export total := Counter.total
                export max := Counter.MAX
            )"
        ))
        {
            t.Run();
            Check("class Counter c1.val == 10", t.GetExported("c1Val").Is<int64>() && t.GetExported("c1Val").Get<int64>() == 10);
            Check("class Counter c2.val == 0", t.GetExported("c2Val").Is<int64>() && t.GetExported("c2Val").Get<int64>() == 0);
            Check("static total == 2", t.GetExported("total").Is<int64>() && t.GetExported("total").Get<int64>() == 2);
            Check("static const MAX == 100", t.GetExported("max").Is<int64>() && t.GetExported("max").Get<int64>() == 100);
        }
        else
        {
            Check("Class script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Inheritance ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                class Animal
                    name : string = ""
                    Speak := () -> string
                        return "..."
                    end
                end

                class Dog : Animal
                    Dog()
                        self.name = "Dog"
                    end
                    Speak := () -> string
                        return "Woof"
                    end
                end

                a := new Dog
                export aName := a.name
                export aSound := a.Speak()
            )"
        ))
        {
            t.Run();
            Check("inheritance: Dog.name == 'Dog'", t.GetExported("aName").Is<String>() && t.GetExported("aName").Get<String>() == "Dog");
            Check("inheritance: Dog.Speak() == 'Woof'", t.GetExported("aSound").Is<String>() && t.GetExported("aSound").Get<String>() == "Woof");
        }
        else
        {
            Check("Inheritance script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Enums and switch ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                enum Color : uint8
                    Red = 0,
                    Green,
                    Blue
                end

                c := Color.Green

                export cName := string(c)

                switchResult := ""
                switch c
                    case 0:
                        switchResult = "Red"
                    case 1:
                        switchResult = "Green"
                    default:
                        switchResult = "Unknown"
                end
                export switchResult

                export switchExpr := switch c
                    case Color.Red: "red"; break
                    case Color.Green: "green"; break
                    default: "other"; break
                end
            )"
        ))
        {
            t.Run();
            Check("enum Color.Green stringified", t.GetExported("cName").Is<String>());
            Check("switch case 1 -> Green", t.GetExported("switchResult").Is<String>() && t.GetExported("switchResult").Get<String>() == "Green");
            Check("switch expression -> green", t.GetExported("switchExpr").Is<String>() && t.GetExported("switchExpr").Get<String>() == "green");
        }
        else
        {
            Check("Enum/switch script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── References ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                foo := 9
                ref fooRef = foo
                foo2 := fooRef
                fooRef = 10

                export fooVal := foo
                export fooRefVal := fooRef
                export foo2Val := foo2

                mutateRef := func(ref inRef : int)
                    inRef += 2
                end
                mutateRef(foo)
                export afterMutate := foo
            )"
        ))
        {
            t.Run();
            Check("ref: foo == 10 (mutated via ref)", t.GetExported("fooVal").Is<int64>() && t.GetExported("fooVal").Get<int64>() == 10);
            Check("ref: fooRef == 10", t.GetExported("fooRefVal").Is<int64>() && t.GetExported("fooRefVal").Get<int64>() == 10);
            Check("ref: foo2 == 9 (copy)", t.GetExported("foo2Val").Is<int64>() && t.GetExported("foo2Val").Get<int64>() == 9);
            Check("ref arg: foo == 12 after mutateRef", t.GetExported("afterMutate").Is<int64>() && t.GetExported("afterMutate").Get<int64>() == 12);
        }
        else
        {
            Check("Reference script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Maps ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                m := { "a": 1, "b": 2, "c": 3 }
                export ma := m["a"]
                m["b"] = 99
                export mb := m["b"]
                export keyCount := m.Keys().Size()
            )"
        ))
        {
            t.Run();
            Check("map['a'] == 1", t.GetExported("ma").Is<int64>() && t.GetExported("ma").Get<int64>() == 1);
            Check("map['b'] == 99 (mutated)", t.GetExported("mb").Is<int64>() && t.GetExported("mb").Get<int64>() == 99);
            Check("map has 3 keys", t.GetExported("keyCount").Is<int64>() && t.GetExported("keyCount").Get<int64>() == 3);
        }
        else
        {
            Check("Map script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Inc/dec operators, compound assignment ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                a := 5
                a++
                export postInc := a
                b := 5
                ++b
                export preInc := b
                c := 10
                c--
                export postDec := c
                d := 10
                --d
                export preDec := d
                e := 7
                e += 3
                export plusEq := e
                f := 20
                f -= 5
                export minusEq := f
            )"
        ))
        {
            t.Run();
            Check("post-inc: a++ => 6", t.GetExported("postInc").Is<int64>() && t.GetExported("postInc").Get<int64>() == 6);
            Check("pre-inc: ++b => 6", t.GetExported("preInc").Is<int64>() && t.GetExported("preInc").Get<int64>() == 6);
            Check("post-dec: c-- => 9", t.GetExported("postDec").Is<int64>() && t.GetExported("postDec").Get<int64>() == 9);
            Check("pre-dec: --d => 9", t.GetExported("preDec").Is<int64>() && t.GetExported("preDec").Get<int64>() == 9);
            Check("+= 3: 7+3=10", t.GetExported("plusEq").Is<int64>() && t.GetExported("plusEq").Get<int64>() == 10);
            Check("-= 5: 20-5=15", t.GetExported("minusEq").Is<int64>() && t.GetExported("minusEq").Get<int64>() == 15);
        }
        else
        {
            Check("Inc/dec script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Name literals ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export n1 := #testName
                export n2 := #hello
            )"
        ))
        {
            t.Run();
            Check("name literal #testName is valid", t.GetExported("n1").IsValid());
            Check("name literal #hello is valid", t.GetExported("n2").IsValid());
        }
        else
        {
            Check("Name literal script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Exception handling ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                caught := false
                try
                    throw "test exception"
                catch
                    caught = true
                end
                export caught

                safeDiv := (a : int, b : int) -> int
                    if (b == 0)
                        throw "division by zero"
                    end
                    return a / b
                end

                result := 0
                hadError := false
                try
                    result = safeDiv(10, 0)
                catch
                    hadError = true
                end
                export hadError
                export resultAfterError := result
            )"
        ))
        {
            t.Run();
            Check("try/catch caught throw", t.GetExported("caught").Is<bool>() && t.GetExported("caught").Get<bool>() == true);
            Check("try/catch caught function throw", t.GetExported("hadError").Is<bool>() && t.GetExported("hadError").Get<bool>() == true);
            Check("result unchanged after error", t.GetExported("resultAfterError").Is<int64>() && t.GetExported("resultAfterError").Get<int64>() == 0);
        }
        else
        {
            Check("Exception script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Break and continue ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                sum := 0
                for (i := 0; i < 10; i++)
                    if (i == 3)
                        continue
                    end
                    if (i == 7)
                        break
                    end
                    sum += i
                end
                export sum
            )"
        ))
        {
            t.Run();
            Check("break/continue: sum {0,1,2,4,5,6} == 18", t.GetExported("sum").Is<int64>() && t.GetExported("sum").Get<int64>() == 18);
        }
        else
        {
            Check("Break/continue script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Array<struct> and nested assignment ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                struct Item
                    label : string = ""
                    value : int = 0
                end

                items : Array<Item> = [new Item, new Item]
                items[0].label = "first"
                items[0].value = 42
                items[1].label = "second"
                items[1].value = 99

                items[0].value = 100

                export label0 := items[0].label
                export val0 := items[0].value
                export label1 := items[1].label
                export val1 := items[1].value
            )"
        ))
        {
            t.Run();
            Check("struct array[0].label == 'first'", t.GetExported("label0").Is<String>() && t.GetExported("label0").Get<String>() == "first");
            Check("struct array[0].value == 100 (mutated)", t.GetExported("val0").Is<int64>() && t.GetExported("val0").Get<int64>() == 100);
            Check("struct array[1].label == 'second'", t.GetExported("label1").Is<String>() && t.GetExported("label1").Get<String>() == "second");
            Check("struct array[1].value == 99", t.GetExported("val1").Is<int64>() && t.GetExported("val1").Get<int64>() == 99);
        }
        else
        {
            Check("Array struct script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    // ── Null and null checks ──
    {
        ScriptTest t;
        if (t.Compile(
            R"(
                export n := null
                export isNull := n == null
                export isNotNull := n != null
            )"
        ))
        {
            t.Run();
            Check("null is null", t.GetExported("isNull").Is<bool>() && t.GetExported("isNull").Get<bool>() == true);
            Check("null != null is false", t.GetExported("isNotNull").Is<bool>() && t.GetExported("isNotNull").Get<bool>() == false);
        }
        else
        {
            Check("Null script compiles", false, "Compilation failed");
        }
        t.Destroy();
    }

    HYP_LOG(Engine, Info, "========== Script Test Results: {} passed, {} failed ==========",
            g_passCount, g_failCount);

    if (g_failCount > 0)
    {
        HYP_LOG(Engine, Error, "!!! SCRIPT TEST HAD FAILURES !!!");
    }

    HypScript::Shutdown();
}

} // namespace script
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS && HYP_SCRIPT
