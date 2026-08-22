using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorProject")]
    public class EditorProject : ObjectBase
    {
        private static LogChannel _logChannel = LogChannel.ByName("Editor");

        protected override void Dispose(bool isDisposing)
        {
            if (isDisposing && IsValid)
            {
                if (GameInstance != null && GameInstance.World == null)
                {
                    World?.Dispose();
                }

                GameInstance?.Dispose();
            }

            base.Dispose(isDisposing);
        }

        public Name Name
        {
            get => this.GetName();
            set => this.SetName(value);
        }

        public World? World => this.GetWorld();

        public Game? GameInstance
        {
            get => this.GetGame();
            set => this.SetGame(value);
        }

        public Time LastSavedTime => this.GetLastSavedTime();

        public string FilePath => this.GetFilePath();

        public bool IsSaved => this.IsSaved();

        public EditorActionStack? ActionStack => this.GetActionStack();

        public Name GetNextDefaultProjectName(string defaultProjectName)
        {
            string projectsDirectory = this.GetProjectsDirectory();
            string[] directories = Array.Empty<string>();

            try
            {
                directories = Directory.GetDirectories(projectsDirectory);
            }
            catch (Exception e)
            {
                Logger.Log(_logChannel, LogLevel.Error, "Failed to get files in projects directory: " + e.Message);
            }

            for (int i = 1; i < int.MaxValue; i++)
            {
                string projectName = defaultProjectName + i;

                bool projectNameExists = false;

                foreach (string directory in directories)
                {
                    // get basename of dir without extension
                    string basename = Path.GetFileNameWithoutExtension(directory);

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