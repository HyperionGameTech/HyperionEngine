using System;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class AddNewBakeLayerPanelViewModel : EditorPanelViewModel
    {
        private readonly Action<string?> _onCompleted;

        private string _layerName = "NewLayer";

        public string LayerName
        {
            get => _layerName;
            set => SetProperty(ref _layerName, value);
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public AddNewBakeLayerPanelViewModel(Action<string?> onCompleted)
            : base("New Bake Layer")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private void OnConfirm()
        {
            _onCompleted(LayerName);
            PanelService.Instance.ClosePanel();
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.ClosePanel();
        }
    }
}
