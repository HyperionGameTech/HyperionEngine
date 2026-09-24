using System;
using System.IO;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class RecentProjectItemViewModel : ViewModelBase
    {
        public string FilePath { get; }
        public string Name { get; }
        public string Directory { get; }
        public bool Exists { get; }
        public string LastModifiedText { get; }

        public RecentProjectItemViewModel(string filePath)
        {
            FilePath = filePath;
            Name = RecentProjectsService.GetProjectName(filePath);
            Directory = Path.GetDirectoryName(filePath) ?? string.Empty;
            Exists = File.Exists(filePath);
            LastModifiedText = Exists ? FormatLastModified(File.GetLastWriteTime(filePath)) : "Missing";
        }

        private static string FormatLastModified(DateTime lastModified)
        {
            TimeSpan age = DateTime.Now - lastModified;

            if (age.TotalMinutes < 1)
                return "Just now";
            if (age.TotalHours < 1)
                return $"{(int)age.TotalMinutes} min ago";
            if (age.TotalDays < 1)
                return $"{(int)age.TotalHours} hr ago";
            if (age.TotalDays < 7)
                return $"{(int)age.TotalDays} day{((int)age.TotalDays == 1 ? "" : "s")} ago";

            return lastModified.ToString("MMM d, yyyy");
        }
    }
}
