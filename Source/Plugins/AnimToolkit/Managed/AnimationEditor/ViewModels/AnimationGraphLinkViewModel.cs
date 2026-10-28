using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class AnimationGraphLinkViewModel : InspectorPropertyViewModelBase
    {
        private readonly Entity _entity;

        private string _graphName = string.Empty;
        private bool _hasGraph;

        public AnimationGraphLinkViewModel(Entity entity)
            : base("Graph", default, null, null, false)
        {
            _entity = entity;

            OpenCommand = new RelayCommand(() => AnimationEditorWiring.OpenEntityGraph(_entity));
        }

        public override bool SupportsMultipleTargets => false;

        public ICommand OpenCommand { get; }

        public string GraphName
        {
            get => _graphName;
            private set => SetProperty(ref _graphName, value);
        }

        public bool HasGraph
        {
            get => _hasGraph;
            private set => SetProperty(ref _hasGraph, value);
        }

        public override void RefreshValue()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                AnimationGraph? graph = AnimationEditorWiring.GetState()?.FindEntityGraph(_entity);
                string name = graph?.Name.ToString() ?? string.Empty;

                Dispatcher.UIThread.Post(() =>
                {
                    GraphName = name;
                    HasGraph = graph != null;
                });
            });
        }
    }
}
