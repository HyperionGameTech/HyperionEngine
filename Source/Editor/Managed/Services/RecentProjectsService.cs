using System;
using System.Collections.Generic;
using System.IO;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.Services
{
    public sealed class RecentProjectsService
    {
        private static RecentProjectsService? _instance;

        public static RecentProjectsService Instance => _instance ??= new RecentProjectsService();

        private readonly DelegateHandler _recentProjectsChangedHandler;
        private DelegateHandler? _editorLaunchedHandler;
        private readonly List<Action<EditorSubsystem>> _pendingEditorActions = new();

        /// Raised on the UI thread whenever the recent list changes
        public event Action? Changed;

        private RecentProjectsService()
        {
            _recentProjectsChangedHandler = EditorState.Instance.GetOnRecentProjectsChangedDelegate().Bind(() =>
            {
                Dispatcher.UIThread.Post(() => Changed?.Invoke());
            });
        }

        public IReadOnlyList<string> RecentProjects
        {
            get
            {
                EditorState editorState = EditorState.Instance;

                int count = editorState.GetNumRecentProjects();
                List<string> recentProjects = new List<string>(count);

                for (int index = 0; index < count; index++)
                {
                    string projectFilepath = editorState.GetRecentProject(index);

                    if (!string.IsNullOrEmpty(projectFilepath))
                    {
                        recentProjects.Add(projectFilepath);
                    }
                }

                return recentProjects;
            }
        }

        public bool AlwaysOpenLastProject
        {
            get => EditorState.Instance.GetAlwaysOpenLastProject();
            set => EditorState.Instance.SetAlwaysOpenLastProject(value);
        }

        /// True when the engine opened the last project by itself at startup, so the start screen is skipped
        public bool WillOpenLastProjectOnStartup => !string.IsNullOrEmpty(EditorState.Instance.GetStartupProjectPath());

        public static string GetProjectName(string projectFilepath) => Path.GetFileNameWithoutExtension(projectFilepath);

        public static bool IsSameProject(string projectFilepath, string? otherProjectFilepath)
        {
            if (string.IsNullOrEmpty(projectFilepath) || string.IsNullOrEmpty(otherProjectFilepath))
            {
                return false;
            }

            StringComparison comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;

            try
            {
                return string.Equals(Path.GetFullPath(projectFilepath), Path.GetFullPath(otherProjectFilepath), comparison);
            }
            catch (Exception)
            {
                return string.Equals(projectFilepath, otherProjectFilepath, comparison);
            }
        }

        public void OpenProject(string projectFilepath)
        {
            if (string.IsNullOrEmpty(projectFilepath))
            {
                return;
            }

            RunWhenEditorReady(editorSubsystem => editorSubsystem.OpenProjectAtPath(projectFilepath));
        }

        public void BrowseForProject()
        {
            RunWhenEditorReady(editorSubsystem => editorSubsystem.ExecuteCommandByName(new Name("EditorCommandOpenProject")));
        }

        public void NewProject()
        {
            RunWhenEditorReady(editorSubsystem => editorSubsystem.ExecuteCommandByName(new Name("EditorCommandNewProject")));
        }

        public void Remove(string projectFilepath)
        {
            EditorState.Instance.RemoveRecentProject(projectFilepath);
        }

        public void Clear()
        {
            EditorState.Instance.ClearRecentProjects();
        }

        // The start screen can be used before the editor game has finished launching
        private void RunWhenEditorReady(Action<EditorSubsystem> action)
        {
            EditorGame? editorGame = EngineManager.EditorGame;

            if (editorGame == null)
            {
                Logger.Log(LogLevel.Error, "Editor game instance is not initialized; cannot run project action");

                return;
            }

            if (editorGame.IsLaunched() && editorGame.EditorSubsystem is { } editorSubsystem)
            {
                action(editorSubsystem);

                return;
            }

            lock (_pendingEditorActions)
            {
                _pendingEditorActions.Add(action);

                if (_editorLaunchedHandler != null)
                {
                    return;
                }

                _editorLaunchedHandler = editorGame.GetOnLaunchedDelegate().Bind(() =>
                {
                    Dispatcher.UIThread.Post(RunPendingEditorActions);
                });
            }
        }

        private void RunPendingEditorActions()
        {
            EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;

            List<Action<EditorSubsystem>> actions;

            lock (_pendingEditorActions)
            {
                _editorLaunchedHandler?.Remove();
                _editorLaunchedHandler = null;

                actions = new List<Action<EditorSubsystem>>(_pendingEditorActions);
                _pendingEditorActions.Clear();
            }

            if (editorSubsystem == null)
            {
                Logger.Log(LogLevel.Error, "EditorSubsystem is not available after launch; dropping project actions");

                return;
            }

            foreach (Action<EditorSubsystem> action in actions)
            {
                action(editorSubsystem);
            }
        }
    }
}
