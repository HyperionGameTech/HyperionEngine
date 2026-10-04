using System;
using System.Collections.Generic;
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
        private Func<bool>? _canExecute;

        private static readonly List<WeakReference<EditorCommand>> s_instances = new List<WeakReference<EditorCommand>>();

        public EditorCommand(string name, Func<string?>? argumentProvider = null, Func<bool>? canExecute = null)
        {
            _name = name;
            _argumentProvider = argumentProvider;
            _canExecute = canExecute;

            lock (s_instances)
            {
                s_instances.Add(new WeakReference<EditorCommand>(this));
            }
        }

        public static void RaiseCanExecuteChangedForAll()
        {
            Dispatcher.UIThread.VerifyAccess();

            List<EditorCommand> alive = new List<EditorCommand>();

            lock (s_instances)
            {
                s_instances.RemoveAll(weak => !weak.TryGetTarget(out _));

                foreach (WeakReference<EditorCommand> weak in s_instances)
                {
                    if (weak.TryGetTarget(out EditorCommand? command))
                    {
                        alive.Add(command);
                    }
                }
            }

            foreach (EditorCommand command in alive)
            {
                command.RaiseCanExecuteChanged();
            }
        }

        public bool CanExecute(object? parameter) => !string.IsNullOrEmpty(_name)
            && EngineManager.EditorGame?.EditorSubsystem?.IsSimulating() != true
            && (_canExecute?.Invoke() ?? true);
        public void Execute(object? parameter)
        {
            string? argument = parameter as string;
            if (string.IsNullOrEmpty(argument) && parameter is UUID uuid)
            {
                argument = uuid.ToString();
            }
            if (string.IsNullOrEmpty(argument) && parameter != null)
            {
                argument = parameter.ToString();
            }
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