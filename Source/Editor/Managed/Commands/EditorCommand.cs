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
        private Func<string?>? _argumentProvider;

        public EditorCommand(string name, Func<string?>? argumentProvider = null)
        {
            _name = name;
            _argumentProvider = argumentProvider;
        }

        public bool CanExecute(object? parameter) => !string.IsNullOrEmpty(_name)
            && EngineManager.EditorGame?.EditorSubsystem?.IsSimulating() != true;
        public void Execute(object? parameter)
        {
            string? argument = parameter as string;
            if (string.IsNullOrEmpty(argument))
            {
                argument = _argumentProvider?.Invoke();
            }

            if (!string.IsNullOrEmpty(argument))
            {
                EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommand" + _name), argument);
            }
            else
            {
                EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommand" + _name));
            }
        }
        public event EventHandler? CanExecuteChanged;
        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}