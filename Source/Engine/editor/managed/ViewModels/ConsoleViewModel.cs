using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ConsoleViewModel : ViewModelBase
    {
        public ReadOnlyObservableCollection<LogEntry> Logs => ConsoleService.Instance.Logs;

        private string _commandText;
        public string CommandText
        {
            get => _commandText;
            set => SetProperty(ref _commandText, value);
        }

        public ICommand ExecuteCommand { get; }
        public ICommand ClearCommand { get; }

        public ConsoleViewModel()
        {
            ExecuteCommand = new RelayCommand(Execute);
            ClearCommand = new RelayCommand(Clear);
        }

        private void Execute()
        {
            if (string.IsNullOrWhiteSpace(CommandText)) return;

            ConsoleService.Instance.ExecuteCommand(CommandText);
            CommandText = string.Empty;
        }

        private void Clear()
        {
            ConsoleService.Instance.ClearLogs();
        }
    }
}
