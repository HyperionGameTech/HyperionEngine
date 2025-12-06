using System;
using System.Collections.ObjectModel;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class MainWindowViewModel : ViewModelBase
    {
        public class EditorCommand : ICommand
        {
            private string _name;
            
            public EditorCommand(string name)
            {
                _name = name;
            }

            public bool CanExecute(object? parameter) => !string.IsNullOrEmpty(_name);
            public void Execute(object? parameter) => EngineManager.GameInstance?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommand" + _name));
            public event EventHandler? CanExecuteChanged;
            public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }

        private string _title = "Hyperion Editor";
        public string Title
        {
            get => _title;
            set => SetProperty(ref _title, value);
        }
        public SceneHierarchyViewModel SceneHierarchy { get; }
        public InspectorViewModel Inspector { get; }

        public EditorCommand NewProject => new EditorCommand("NewProject");
        public EditorCommand OpenProject => new EditorCommand("OpenProject");
        public EditorCommand SaveProject => new EditorCommand("SaveProject");
        public EditorCommand Exit => new EditorCommand("Exit");
        public EditorCommand Undo => new EditorCommand("Undo");
        public EditorCommand Redo => new EditorCommand("Redo");
        public EditorCommand Copy => new EditorCommand("Copy");
        public EditorCommand Paste => new EditorCommand("Paste");

        public EditorCommand AddEmptyNode => new EditorCommand("AddEmptyNode");
        public EditorCommand AddEntity => new EditorCommand("AddEntity");
        public EditorCommand AddCamera => new EditorCommand("AddCamera");

        public EditorCommand AddPointLight => new EditorCommand("AddPointLight");
        public EditorCommand AddDirectionalLight => new EditorCommand("AddDirectionalLight");
        public EditorCommand AddSpotLight => new EditorCommand("AddSpotLight");
        public EditorCommand AddAreaRectLight => new EditorCommand("AddAreaRectLight");

        public EditorCommand AddLightmapVolume => new EditorCommand("AddLightmapVolume");
        public EditorCommand AddParticleVolume => new EditorCommand("AddParticleVolume");
        public EditorCommand AddReflectionProbe => new EditorCommand("AddReflectionProbe");

        private const int GameLaunchWaitIntervalMs = 500;
        private const int MaxGameLaunchWaitTimeMs = 60000; // max before giving up

        public MainWindowViewModel()
        {
            SceneHierarchy = new SceneHierarchyViewModel();
            Inspector = new InspectorViewModel();

            SceneHierarchy.SelectedNodeChanged += node =>
            {
                Dispatcher.UIThread.Invoke(() => Inspector.SetSelectedNode(node));
            };

            Game? gameInstance = EngineManager.GameInstance;
            if (gameInstance == null)
            {
                throw new InvalidOperationException("Game instance is not initialized.");
            }

            int waitedTime = 0;

            while (!gameInstance.IsLaunched())
            {
                Logger.Log(LogType.Info, "Waiting for game to launch...");

                Thread.Sleep(GameLaunchWaitIntervalMs);

                waitedTime += GameLaunchWaitIntervalMs;

                if (waitedTime >= MaxGameLaunchWaitTimeMs)
                {
                    throw new TimeoutException("Timed out waiting for game to launch!");
                }
            }

            World? world = gameInstance.World;
            if (world == null)
            {
                throw new InvalidOperationException("Game world is not initialized.");
            }

            EditorSubsystem? editorSubsystem = world.GetSubsystem<EditorSubsystem>();
            if (editorSubsystem == null)
            {
                throw new InvalidOperationException("EditorSubsystem is not available in the world.");
            }

            Scene? activeScene = editorSubsystem.GetActiveScene();
            if (activeScene != null)
            {
                SceneHierarchy.AttachToScene(activeScene);
            }

            // handle active scene changes
            editorSubsystem.GetOnActiveSceneChangedDelegate()
                .Bind(HandleActiveSceneChanged)
                .Detach();
        }

        private void HandleActiveSceneChanged(Scene? scene)
        {
            if (scene == null)
            {
                return;
            }

            Dispatcher.UIThread.Invoke(() =>
            {
                SceneHierarchy.AttachToScene(scene);
            });
        }
    }
}