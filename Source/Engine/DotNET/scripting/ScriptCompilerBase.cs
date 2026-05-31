using System;

namespace Hyperion
{
    public abstract class ScriptCompilerBase
    {
        protected static LogChannel logChannel = LogChannel.ByName("ScriptCompiler");

        protected string sourceDirectory;
        protected string intermediateDirectory;
        protected string binaryOutputDirectory;

        protected const int timeoutMilliseconds = 30000; // 30 seconds

        public ScriptCompilerBase(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
        {
            this.sourceDirectory = sourceDirectory;
            this.intermediateDirectory = intermediateDirectory;
            this.binaryOutputDirectory = binaryOutputDirectory;

            foreach (string directory in new string[] { sourceDirectory, intermediateDirectory, binaryOutputDirectory })
            {
                try
                {
                    CreateDirectoryIfNotExist(directory);
                }
                catch (Exception e)
                {
                    Logger.Log(logChannel, LogLevel.Error, "Failed to create directory {0}: {1}", directory, e.Message);
                }
            }
        }

        protected void CreateDirectoryIfNotExist(string directory)
        {
            if (!System.IO.Directory.Exists(directory))
            {
                System.IO.Directory.CreateDirectory(directory);
            }
        }

        public abstract void BuildAllProjects();
        public abstract bool Compile(ref ScriptDesc scriptDesc);
    }
}
