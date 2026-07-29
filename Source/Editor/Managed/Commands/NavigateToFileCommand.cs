using System;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.Commands
{

    public class NavigateToFileCommand : ICommand
    {
        public static readonly NavigateToFileCommand DefaultInstance = new NavigateToFileCommand();

        public NavigateToFileCommand()
        {
        }

        public bool CanExecute(object? parameter) => parameter is LogEntry logEntry && !string.IsNullOrEmpty(logEntry.FileName);

        public void Execute(object? parameter)
        {
            if (parameter is not LogEntry logEntry)
            {
                return;
            }

            CodeEditorService.OpenFile(logEntry.FileName, logEntry.LineNumber);
        }

        public event EventHandler? CanExecuteChanged;

        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}