using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorProject")]
    public class EditorProject : ObjectBase
    {
        private static LogChannel logChannel = LogChannel.ByName("Editor");

        public Uuid Uuid
        {
            get
            {
                return this.GetUUID();
            }
        }

        public Name Name
        {
            get
            {
                return this.GetName();
            }
            set
            {
                this.SetName(value);
            }
        }

        public World World
        {
            get
            {
                return this.GetWorld();
            }
        }

        public Time LastSavedTime
        {
            get
            {
                return this.GetLastSavedTime();
            }
        }

        public string FilePath
        {
            get
            {
                return this.GetFilePath();
            }
        }

        public bool IsSaved
        {
            get
            {
                return this.IsSaved();
            }
        }

        public Name GetNextDefaultProjectName(string defaultProjectName)
        {
            string projectsDirectory = this.GetProjectsDirectory();
            string[] directories = Array.Empty<string>();

            try
            {
                directories = System.IO.Directory.GetDirectories(projectsDirectory);
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogType.Error, "Failed to get files in projects directory: " + e.Message);
            }

            for (int i = 1; i < int.MaxValue; i++)
            {
                string projectName = defaultProjectName + i;

                bool projectNameExists = false;

                foreach (string directory in directories)
                {
                    // get basename of dir without extension
                    string basename = System.IO.Path.GetFileNameWithoutExtension(directory);

                    if (basename.Equals(projectName, StringComparison.OrdinalIgnoreCase))
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