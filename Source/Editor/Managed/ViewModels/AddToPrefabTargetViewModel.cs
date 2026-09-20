using System;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class AddToPrefabTargetViewModel : ViewModelBase
    {
        private readonly string _prefabName;

        public AddToPrefabTargetViewModel(string prefabName)
        {
            _prefabName = prefabName ?? throw new ArgumentNullException(nameof(prefabName));

            AddToPrefabCommand = new RelayCommand(Execute);
        }

        public string PrefabName => _prefabName;

        public ICommand AddToPrefabCommand { get; }

        private void Execute()
        {
            EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommandAddToPrefab"), _prefabName);
        }
    }
}
