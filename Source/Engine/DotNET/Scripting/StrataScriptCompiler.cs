using System;

namespace Hyperion
{
    public class StrataScriptCompiler : ScriptCompilerBase
    {
        public StrataScriptCompiler(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
            : base(sourceDirectory, intermediateDirectory, binaryOutputDirectory)
        {
        }

        public override void BuildAllProjects()
        {
        }

        public override bool Compile(ref ScriptDesc scriptDesc)
        {
            return true;
        }
    }
}
