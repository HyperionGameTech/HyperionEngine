using System;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace Hyperion
{
    /// <summary>
    /// Uses Roslyn to compile C# scripts into DLLs
    /// </summary>
    public class ScriptCompiler
    {
        public ScriptCompiler()
        {

        }

        // just testing
        public void Compile(string scriptPath)
        {
            // Console.WriteLine("Compiling script at {0}", scriptPath);

            // string scriptCode = System.IO.File.ReadAllText(scriptPath);

            // ScriptOptions options = ScriptOptions.Default
            //     .WithReferences(typeof(System.Object).Assembly)
            //     .WithReferences(typeof(System.Console).Assembly)
            //     .WithImports("System");

            // CSharpScript.Create(scriptCode, options).Compile();

            var tree = CSharpSyntaxTree.ParseText(@"
                using System;
                public class C
                {
                    public static void Main()
                    {
                        Console.WriteLine(""Hello World!"");
                        Console.ReadLine();
                    }   
                }");

                var mscorlib = MetadataReference.CreateFromFile(typeof(object).Assembly.Location);
                var compilation = CSharpCompilation.Create("MyCompilation",
                    syntaxTrees: new[] { tree }, references: new[] { mscorlib });

                //Emitting to file is available through an extension method in the Microsoft.CodeAnalysis namespace
                var emitResult = compilation.Emit("output.exe", "output.pdb");

                //If our compilation failed, we can discover exactly why.
                if(!emitResult.Success)
                {
                    foreach(var diagnostic in emitResult.Diagnostics)
                    {
                        Console.WriteLine(diagnostic.ToString());
                    }
                }
        }
    }
}