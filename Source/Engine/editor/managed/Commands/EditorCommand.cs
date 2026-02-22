using System;
using System.Diagnostics;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.Commands
{
    public class EditorCommand : ICommand
    {
        private string _name;
        
        public EditorCommand(string name)
        {
            _name = name;
        }

        public bool CanExecute(object? parameter) => !string.IsNullOrEmpty(_name);
        public void Execute(object? parameter) => EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommand" + _name));
        public event EventHandler? CanExecuteChanged;
        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}