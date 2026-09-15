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
        public bool AutoLaunchServer { get; }
        public uint CachePort { get; }

        public NetworkSettingsResult(string host, uint port, bool autoLaunchServer, uint cachePort)
        {
            Host = host;
            Port = port;
            AutoLaunchServer = autoLaunchServer;
            CachePort = cachePort;
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

        private bool _autoLaunchServer;
        public bool AutoLaunchServer
        {
            get => _autoLaunchServer;
            set => SetProperty(ref _autoLaunchServer, value);
        }

        private string _cachePort;
        public string CachePort
        {
            get => _cachePort;
            set
            {
                if (SetProperty(ref _cachePort, value))
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

        public NetworkSettingsPanelViewModel(string host, uint port, bool autoLaunchServer, uint cachePort, Action<NetworkSettingsResult?> onCompleted)
            : base("Network Settings")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            _host = host;
            _port = port.ToString();
            _autoLaunchServer = autoLaunchServer;
            _cachePort = cachePort.ToString();

            ConfirmCommand = new RelayCommand(OnConfirm, () => !HasError);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private static bool TryParsePort(string text, out ushort port)
        {
            return ushort.TryParse(text.Trim(), out port) && port != 0;
        }

        private void Validate()
        {
            if (string.IsNullOrWhiteSpace(_host))
            {
                ErrorText = "Host cannot be empty.";
            }
            else if (!TryParsePort(_port, out ushort gamePort))
            {
                ErrorText = "Port must be a number between 1 and 65535.";
            }
            else if (!TryParsePort(_cachePort, out ushort cachePort))
            {
                ErrorText = "Cache server port must be a number between 1 and 65535.";
            }
            else if (gamePort == cachePort)
            {
                ErrorText = "Port and cache server port must be different.";
            }
            else
            {
                ErrorText = string.Empty;
            }

            (ConfirmCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        private void OnConfirm()
        {
            Validate();

            if (HasError || !TryParsePort(_port, out ushort port) || !TryParsePort(_cachePort, out ushort cachePort))
            {
                return;
            }

            _onCompleted(new NetworkSettingsResult(_host.Trim(), port, _autoLaunchServer, cachePort));
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(null);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
