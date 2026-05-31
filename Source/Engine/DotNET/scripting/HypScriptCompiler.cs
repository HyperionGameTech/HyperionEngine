using System;

namespace Hyperion
{
    public class HypScriptCompiler : ScriptCompilerBase
    {
        public HypScriptCompiler(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
            : base(sourceDirectory, intermediateDirectory, binaryOutputDirectory)
        {
        }

        public override void BuildAllProjects()
        {
            // HypScript compilation happens natively in C++ via the StateChanged callback
        }

        public override bool Compile(ref ScriptDesc scriptDesc)
        {
            // HypScript compilation happens natively in C++ via the StateChanged callback
            return true;
        }
    }
}
