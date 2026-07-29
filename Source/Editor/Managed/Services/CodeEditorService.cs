using System;
using System.Diagnostics;

namespace Hyperion.Editor.Services
{
    public static class CodeEditorService
    {
        private const string DefaultEditor = "VSCode";

        public static void OpenFile(string filePath, int lineNumber = 0)
        {
            if (string.IsNullOrWhiteSpace(filePath))
            {
                return;
            }

            string? commandStr = BuildCommandString(GetEditorName(), filePath, lineNumber);
            ProcessStartInfo? startInfo = commandStr == null ? null : BuildStartInfo(commandStr);

            if (startInfo == null)
            {
                return;
            }

            try
            {
                using Process process = Process.Start(startInfo)!;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Failed to open file in code editor: {ex.Message}");
            }
        }

        private static string GetEditorName()
        {
            string? editor = EngineManager.EditorGame?.EditorSubsystem?.GetCodeEditor();

            return string.IsNullOrWhiteSpace(editor) ? DefaultEditor : editor;
        }

        private static string? BuildCommandString(string editor, string filePath, int lineNumber)
        {
            bool hasLine = lineNumber > 0;

            switch (editor.Trim().ToLowerInvariant())
            {
                case "zed":
                    return hasLine
                        ? $"zed \"{filePath}:{lineNumber}\""
                        : $"zed \"{filePath}\"";

                case "notepad++":
                case "notepadpp":
                    return hasLine
                        ? $"notepad++ -n{lineNumber} \"{filePath}\""
                        : $"notepad++ \"{filePath}\"";

                case "vscode":
                case "vs code":
                case "code":
                default:
                    return hasLine
                        ? $"code --goto \"{filePath}:{lineNumber}\""
                        : $"code \"{filePath}\"";
            }
        }

        private static ProcessStartInfo? BuildStartInfo(string commandStr)
        {
            if (OperatingSystem.IsWindows())
            {
                return new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = "/C " + commandStr,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
            }

            if (OperatingSystem.IsLinux())
            {
                return new ProcessStartInfo
                {
                    FileName = "/bin/bash",
                    Arguments = "-c \"" + commandStr.Replace("\"", "\\\"") + "\"",
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
            }

            if (OperatingSystem.IsMacOS())
            {
                return new ProcessStartInfo
                {
                    FileName = "/bin/zsh",
                    Arguments = "-l -c \"" + commandStr.Replace("\"", "\\\"") + "\"",
                    UseShellExecute = false,
                    CreateNoWindow = true
                };
            }

            return null;
        }
    }
}
