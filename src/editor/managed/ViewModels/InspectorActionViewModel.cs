using System;
using System.Windows.Input;
using System.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorActionViewModel : ViewModelBase
    {
        private readonly ObjectBase _target;
        private readonly Method _method;
        private readonly RelayCommand _executeCommand;
        private int _isExecuting;

        public InspectorActionViewModel(ObjectBase? target, Method method, string label, bool isEnabled = true)
        {
            _target = target ?? throw new ArgumentNullException(nameof(target));
            _method = method;
            _isExecuting = 0;

            if (string.IsNullOrWhiteSpace(label))
            {
                throw new ArgumentException("Label cannot be null or whitespace.", nameof(label));
            }

            Label = label;
            IsEnabled = isEnabled;

            _executeCommand = new RelayCommand(Execute, CanExecute);
        }

        public string Label { get; }

        public bool IsEnabled { get; }

        public ICommand ExecuteActionCommand => _executeCommand;

        public void RefreshCanExecute()
        {
            _executeCommand.RaiseCanExecuteChanged();
        }

        private bool CanExecute()
        {
            return IsEnabled && _target.IsValid;
        }

        private void Execute()
        {
            if (Interlocked.CompareExchange(ref _isExecuting, 1, 0) == 1)
            {
                // already executing
                return;
            }
            
            // always execute on the sim thread
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue _ = _method.Invoke(_target);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector action '{Label}' failed: {ex.Message}");
                }
                finally
                {
                    _isExecuting = 0;
                }
            });
        }
    }
}
