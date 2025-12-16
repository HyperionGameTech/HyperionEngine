using System.Collections.ObjectModel;

namespace Hyperion.Editor.ViewModels
{
    public class SceneNodeViewModel : ViewModelBase
    {
        private string _name;
        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        public ObservableCollection<SceneNodeViewModel> Children { get; } = new ObservableCollection<SceneNodeViewModel>();

        public SceneNodeViewModel(string name)
        {
            _name = name;
        }
    }
}
