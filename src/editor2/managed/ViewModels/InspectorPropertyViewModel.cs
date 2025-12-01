using System.Collections.ObjectModel;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorPropertyViewModel : ViewModelBase
    {
        private string _name;
        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        private object? _value;
        public object? Value
        {
            get => _value;
            set => SetProperty(ref _value, value);
        }

        public InspectorPropertyViewModel(string name, object? value = null)
        {
            _name = name;
            _value = value;
        }
    }
}
