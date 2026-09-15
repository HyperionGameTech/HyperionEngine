using System;
using System.Windows.Input;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public sealed class NetworkSettingsResult
    {
        public string Host { get; }
        public uint Port { get; }

        public NetworkSettingsResult(string host, uint port)
        {
            Host = host;
            Port = port;
        }
    }

    public class NetworkSettingsPanelViewModel : EditorPanelViewModel
    {
        private readonly Action<NetworkSettingsResult?> _onCompleted;

        private string _host;
        public string Host
        {
            get => _host;
            set
            {
                if (SetProperty(ref _host, value))
                {
                    Validate();
                }
            }
        }

        private string _port;
        public string Port
        {
            get => _port;
            set
            {
                if (SetProperty(ref _port, value))
                {
                    Validate();
                }
            }
        }

        private string _errorText = string.Empty;
        public string ErrorText
        {
            get => _errorText;
            private set
            {
                if (SetProperty(ref _errorText, value))
                {
                    OnPropertyChanged(nameof(HasError));
                }
            }
        }

        public bool HasError => !string.IsNullOrEmpty(_errorText);

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public NetworkSettingsPanelViewModel(string host, uint port, Action<NetworkSettingsResult?> onCompleted)
            : base("Network Settings")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            _host = host;
            _port = port.ToString();

            ConfirmCommand = new RelayCommand(OnConfirm, () => !HasError);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private bool TryParsePort(out ushort port)
        {
            return ushort.TryParse(_port.Trim(), out port) && port != 0;
        }

        private void Validate()
        {
            if (string.IsNullOrWhiteSpace(_host))
            {
                ErrorText = "Host cannot be empty.";
            }
            else if (!TryParsePort(out _))
            {
                ErrorText = "Port must be a number between 1 and 65535.";
            }
            else
            {
                ErrorText = string.Empty;
            }

            (ConfirmCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        private void OnConfirm()
        {
            if (string.IsNullOrWhiteSpace(_host) || !TryParsePort(out ushort port))
            {
                return;
            }

            _onCompleted(new NetworkSettingsResult(_host.Trim(), port));
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
