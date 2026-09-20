using System;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class SaveAsPrefabPanelViewModel : EditorPanelViewModel
    {
        // Invoked with the prefab name, or null on cancel.
        private readonly Action<string?> _onCompleted;

        private string _prefabName = "NewPrefab";

        public string PrefabName
        {
            get => _prefabName;
            set => SetProperty(ref _prefabName, value);
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public SaveAsPrefabPanelViewModel(Action<string?> onCompleted)
            : base("Save as Prefab")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private void OnConfirm()
        {
            string name = (PrefabName ?? string.Empty).Trim();

            if (string.IsNullOrWhiteSpace(name) || name.Contains(' '))
            {
                return;
            }

            _onCompleted(name);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
