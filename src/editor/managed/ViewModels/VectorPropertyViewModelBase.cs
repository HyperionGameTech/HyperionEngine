using System;
using System.Globalization;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public abstract class VectorPropertyViewModelBase<TStruct> : InspectorPropertyViewModelBase where TStruct : struct
    {
        private readonly int _componentCount;
        private readonly Func<TStruct, int, float> _getComponent;
        private readonly Func<TStruct, int, float, TStruct> _withComponent;
        private readonly Func<TStruct> _readStruct;
        private readonly Action<TStruct> _writeStruct;

        private readonly string[] _components;

        protected VectorPropertyViewModelBase(
            ObjectBase target,
            Property property,
            bool isReadOnly,
            int componentCount,
            Func<TStruct, int, float> getComponent,
            Func<TStruct, int, float, TStruct> withComponent,
            Func<TStruct>? readOverride = null,
            Action<TStruct>? writeOverride = null)
            : base(target, property, isReadOnly)
        {
            _componentCount = componentCount;
            _getComponent = getComponent;
            _withComponent = withComponent;
            _readStruct = readOverride ?? ReadStructFromProperty;
            _writeStruct = writeOverride ?? WriteStructToProperty;
            _components = new string[_componentCount];
        }

        public string X
        {
            get => _components[0];
            set => OnComponentChanged(0, value);
        }

        public string Y
        {
            get => _componentCount > 1 ? _components[1] : string.Empty;
            set
            {
                if (_componentCount > 1)
                {
                    OnComponentChanged(1, value);
                }
            }
        }

        public string Z
        {
            get => _componentCount > 2 ? _components[2] : string.Empty;
            set
            {
                if (_componentCount > 2)
                {
                    OnComponentChanged(2, value);
                }
            }
        }

        public string W
        {
            get => _componentCount > 3 ? _components[3] : string.Empty;
            set
            {
                if (_componentCount > 3)
                {
                    OnComponentChanged(3, value);
                }
            }
        }

        public override void RefreshValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            try
            {
                if (!TryReadStruct(out TStruct vector))
                {
                    Value = "(unavailable)";
                    ResetComponentStrings();
                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    _isRefreshing = 0;

                    for (int i = 0; i < _componentCount; i++)
                    {
                        _components[i] = FormatComponent(_getComponent(vector, i));
                    }

                    Value = BuildDisplayString(_components);
                    RaiseAllComponents();
                });
            }
            catch (Exception ex)
            {
                _isRefreshing = 0;

                Logger.Log(LogType.Warn, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
            }
        }

        private bool TryReadStruct(out TStruct vector)
        {
            try
            {
                vector = _readStruct();
                return true;
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Warn, $"Inspector failed to read vector for '{_property.Name}': {ex.Message}");
                vector = default;
                return false;
            }
        }

        private void OnComponentChanged(int index, string newValue)
        {
            if (_isRefreshing == 1)
            {
                return;
            }

            if (!_target.IsValid)
            {
                return;
            }

            if (!float.TryParse(newValue, NumberStyles.Float, CultureInfo.InvariantCulture, out float parsed))
            {
                RefreshValue();
                return;
            }

            try
            {
                if (!TryReadStruct(out TStruct current))
                {
                    return;
                }

                TStruct updated = _withComponent(current, index, parsed);
                _writeStruct(updated);

                RefreshValue();
            }
            catch (Exception ex)
            {
                Logger.Log(LogType.Error, $"Inspector failed to set vector component for '{_property.Name}': {ex.Message}");
                RefreshValue();
            }
        }

        private void ResetComponentStrings()
        {
            for (int i = 0; i < _componentCount; i++)
            {
                _components[i] = string.Empty;
            }
            RaiseAllComponents();
        }

        private void RaiseAllComponents()
        {
            // Re-raise property changed so bindings update.
            OnPropertyChanged(nameof(X));
            if (_componentCount > 1) OnPropertyChanged(nameof(Y));
            if (_componentCount > 2) OnPropertyChanged(nameof(Z));
            if (_componentCount > 3) OnPropertyChanged(nameof(W));
        }

        private string BuildDisplayString(string[] components)
        {
            return $"({string.Join(", ", components)})";
        }

        private static string FormatComponent(float value)
        {
            return value.ToString("F3", CultureInfo.InvariantCulture);
        }

        // Blocking wait on game thread to read the struct value.
        private TStruct ReadStructFromProperty()
        {
            Task<TStruct> task = EngineManager.PostToGameThread<TStruct>(() =>
            {
                using BoxedValue boxed = _property.Get(_target);
                object? raw = boxed.GetValue();

                if (raw is TStruct casted)
                {
                    return casted;
                }

                throw new InvalidOperationException($"Property '{_property.Name}' value is not of expected type {typeof(TStruct).Name}");
            });

            task.Wait();

            return task.Result;
        }

        private void WriteStructToProperty(TStruct value)
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToGameThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(value);
                    _property.Set(_target, boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Error, $"Inspector failed to write property '{_property.Name}': {ex.Message}");
                }
                finally
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        //Value = FormatValue(value);
                    });
                }
            });
        }
    }
}