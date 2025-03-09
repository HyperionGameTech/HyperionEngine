using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="EditorProject")]
    public class EditorProject : HypObject
    {
        private static LogChannel logChannel = LogChannel.ByName("Editor");

        public Name GetNextDefaultProjectName()
        {
            string projectsDirectory = this.GetProjectsDirectory();
            string[] filesInProjectsDirectory = Array.Empty<string>();

            try
            {
                filesInProjectsDirectory = System.IO.Directory.GetFiles(projectsDirectory);
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogType.Error, "Failed to get files in projects directory: " + e.Message);
            }

            string defaultProjectName = "UntitledProject";

            for (int i = 1; i < int.MaxValue; i++)
            {
                string projectName = defaultProjectName + i;

                bool projectNameExists = false;

                foreach (string file in filesInProjectsDirectory)
                {
                    if (file == projectName)
                    {
                        projectNameExists = true;
                        break;
                    }
                }

                if (!projectNameExists)
                {
                    return new Name(projectName);
                }
            }

            return new Name(defaultProjectName);
        }
    }
}