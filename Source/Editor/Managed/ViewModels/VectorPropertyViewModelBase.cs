using System;
using System.Globalization;
using System.Runtime.InteropServices;
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
        private readonly bool _hasWriteOverride;

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
            _hasWriteOverride = writeOverride != null;
            _components = new string[_componentCount];
        }

        protected VectorPropertyViewModelBase(
            IntPtr classAddress,
            Func<IntPtr> targetAddressResolver,
            Property property,
            bool isReadOnly,
            int componentCount,
            Func<TStruct, int, float> getComponent,
            Func<TStruct, int, float, TStruct> withComponent,
            Func<TStruct>? readOverride = null,
            Action<TStruct>? writeOverride = null)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _componentCount = componentCount;
            _getComponent = getComponent;
            _withComponent = withComponent;
            _readStruct = readOverride ?? ReadStructFromProperty;
            _writeStruct = writeOverride ?? WriteStructToProperty;
            _hasWriteOverride = writeOverride != null;
            _components = new string[_componentCount];
        }

        protected VectorPropertyViewModelBase(
            string label,
            TypeInfo typeInfo,
            Func<BoxedValue> getter,
            Action<BoxedValue> setter,
            bool isReadOnly,
            int componentCount,
            Func<TStruct, int, float> getComponent,
            Func<TStruct, int, float, TStruct> withComponent)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
            _componentCount = componentCount;
            _getComponent = getComponent;
            _withComponent = withComponent;
            _readStruct = ReadStructFromProperty;
            _writeStruct = WriteStructToProperty;
            _hasWriteOverride = false;
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
                    _isRefreshing = 0;
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

                Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
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
                Logger.Log(LogLevel.Warning, $"Inspector failed to read vector for '{_property.Name}': {ex.Message}");
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

            if (!IsTargetValid)
            {
                return;
            }


            _components[index] = newValue;
        }

        public override void CommitValue()
        {
            if (!IsTargetValid)
            {
                return;
            }

            TStruct current;

            try
            {
                current = _readStruct();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Inspector failed to read vector for '{_property.Name}': {ex.Message}");
                return;
            }

            TStruct updated = current;

            for (int i = 0; i < _componentCount; i++)
            {
                if (TryParseComponent(_components[i], out float parsed))
                {
                    updated = _withComponent(updated, i, parsed);
                }
            }

            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    if (_hasWriteOverride)
                    {
                        _writeStruct(updated);
                    }
                    else
                    {
                        using BoxedValue boxed = new BoxedValue(updated);
                        CommitPropertyChange($"Set {Label}", boxed);
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to write vector property '{_property.Name}': {ex.Message}");
                }
                finally
                {
                    Dispatcher.UIThread.Post(() => _isRefreshing = 0);
                }
            });
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

        protected virtual string FormatComponent(float value)
        {
            return value.ToString("F3", CultureInfo.InvariantCulture);
        }

        protected virtual bool TryParseComponent(string text, out float result)
        {
            return float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out result);
        }

        // Blocking wait on sim thread to read the struct value.
        private TStruct ReadStructFromProperty()
        {
            var task = EngineManager.PostToSimThread<TStruct>(() =>
            {
                using BoxedValue boxed = GetPropertyValue();
                IntPtr ptr = boxed.Pointer;

                if (ptr == IntPtr.Zero)
                {
                    throw new InvalidOperationException($"Property '{_property.Name}' returned null pointer");
                }

                return Marshal.PtrToStructure<TStruct>(ptr);
            });

            return task.Result;
        }

        private void WriteStructToProperty(TStruct value)
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(value);
                    SetPropertyValue(boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to write property '{_property.Name}': {ex.Message}");
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