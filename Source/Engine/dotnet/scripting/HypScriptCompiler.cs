using System;

namespace Hyperion
{
    public class HypScriptCompiler : ScriptCompilerBase
    {
        private new static LogChannel logChannel = LogChannel.ByName("HypScriptCompiler");

        public HypScriptCompiler(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
            : base(sourceDirectory, intermediateDirectory, binaryOutputDirectory)
        {
            try
            {
                CreateDirectoryIfNotExist(sourceDirectory);
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to create directory {0}: {1}", sourceDirectory, e.Message);
            }
        }

        public override void BuildAllProjects()
        {
        }

        public override bool Compile(ref ScriptDesc scriptDesc)
        {
            // HypScript compilation happens natively in C++ via the ScriptSystem callback

            return false;
        }
    }
}