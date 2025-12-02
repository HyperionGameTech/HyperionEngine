using System;
using System.Collections.ObjectModel;
using System.Threading;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class MainWindowViewModel : ViewModelBase
    {
        private string _title = "Hyperion Editor";
        public string Title
        {
            get => _title;
            set => SetProperty(ref _title, value);
        }
        public SceneHierarchyViewModel SceneHierarchy { get; }
        public InspectorViewModel Inspector { get; }

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

        private void HandleActiveSceneChanged(Scene scene)
        {
            Dispatcher.UIThread.Invoke(() =>
            {
                SceneHierarchy.AttachToScene(scene);
            });
        }
    }
}