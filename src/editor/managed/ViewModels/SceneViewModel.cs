using System.Collections.ObjectModel;

namespace Hyperion.Editor.ViewModels
{
    public class SceneViewModel : ViewModelBase
    {
        private Scene _scene;
        private bool _isActive;

        public Scene Scene => _scene;
        public bool IsActive
        {
            get => _isActive;
            set => SetProperty(ref _isActive, value);
        }

        public string Name
        {
            get => _scene.Name.ToString();
            set
            {
                if (_scene.Name.ToString() != value)
                {
                    _scene.Name = new Name(value);
                    OnPropertyChanged();
                }
            }
        }

        public SceneViewModel(Scene scene, bool isActive = false)
        {
            _scene = scene;
            _isActive = isActive;
        }

        public override bool Equals(object? obj)
        {
            if (obj is SceneViewModel other)
            {
                return _scene == other._scene;
            }

            return false;
        }

        public override int GetHashCode()
        {
            return _scene.GetHashCode();
        }

        public static bool operator ==(SceneViewModel? left, SceneViewModel? right)
        {
            if (left is null && right is null)
                return true;
            if (left is null || right is null)
                return false;
            return left._scene == right._scene;
        }

        public static bool operator !=(SceneViewModel? left, SceneViewModel? right)
        {
            return !(left == right);
        }
    }
}
