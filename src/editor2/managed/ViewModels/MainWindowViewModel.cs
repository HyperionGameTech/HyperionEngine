using System;
using System.Collections.ObjectModel;
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

            World world = gameInstance.World;

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