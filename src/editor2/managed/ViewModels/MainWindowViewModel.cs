using System.Collections.ObjectModel;

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

        public ObservableCollection<SceneNodeViewModel> SceneHierarchy { get; } = new ObservableCollection<SceneNodeViewModel>();
        public ObservableCollection<InspectorPropertyViewModel> InspectorProperties { get; } = new ObservableCollection<InspectorPropertyViewModel>();

        public MainWindowViewModel()
        {
            // Sample data
            var root = new SceneNodeViewModel("Scene Root");

            var camera = new SceneNodeViewModel("Camera");
            root.Children.Add(camera);

            var light = new SceneNodeViewModel("Directional Light");
            root.Children.Add(light);

            var cube = new SceneNodeViewModel("Cube");
            cube.Children.Add(new SceneNodeViewModel("Cube Child"));
            root.Children.Add(cube);

            SceneHierarchy.Add(root);

            // Sample inspector properties
            InspectorProperties.Add(new InspectorPropertyViewModel("Name", "Cube"));
            InspectorProperties.Add(new InspectorPropertyViewModel("Position", "(0, 0, 0)"));
            InspectorProperties.Add(new InspectorPropertyViewModel("Rotation", "(0, 0, 0)"));
            InspectorProperties.Add(new InspectorPropertyViewModel("Scale", "(1, 1, 1)"));
        }
    }
}