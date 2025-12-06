using System;
using System.Collections.ObjectModel;
using System.Windows.Input;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class ConsoleViewModel : ViewModelBase
    {
        public ObservableCollection<LogEntry> Logs => ConsoleService.Instance.Logs;

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
            ConsoleService.Instance.Logs.Clear();
        }
    }

    public class RelayCommand : ICommand
    {
        private readonly Action _execute;
        private readonly Func<bool> _canExecute;

        public RelayCommand(Action execute, Func<bool> canExecute = null)
        {
            _execute = execute ?? throw new ArgumentNullException(nameof(execute));
            _canExecute = canExecute;
        }

        public bool CanExecute(object parameter) => _canExecute == null || _canExecute();

        public void Execute(object parameter) => _execute();

        public event EventHandler CanExecuteChanged;
        
        public void RaiseCanExecuteChanged()
        {
            CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }
    }
}
