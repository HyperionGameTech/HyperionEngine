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

        // Both run on the sim thread.
        private readonly Func<TStruct> _readStruct;
        private readonly Action<TStruct>? _writeStruct;

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
            _writeStruct = writeOverride;
            _components = CreateComponentStrings(componentCount);
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
            _writeStruct = writeOverride;
            _components = CreateComponentStrings(componentCount);
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
            _writeStruct = null;
            _components = CreateComponentStrings(componentCount);
        }

        private static string[] CreateComponentStrings(int count)
        {
            string[] components = new string[count];

            for (int i = 0; i < count; i++)
            {
                components[i] = string.Empty;
            }

            return components;
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
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                TStruct vector;

                try
                {
                    vector = _readStruct();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read vector for '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                string[] formatted = new string[_componentCount];

                for (int i = 0; i < _componentCount; i++)
                {
                    formatted[i] = FormatComponent(_getComponent(vector, i));
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            Value = BuildDisplayString(formatted);

                            // Leave the text boxes alone while the user is typing in them.
                            if (!IsEditing)
                            {
                                Array.Copy(formatted, _components, _componentCount);
                                RaiseAllComponents();
                            }
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        private void OnComponentChanged(int index, string newValue)
        {
            if (IsApplyingModelValue)
            {
                return;
            }

            _components[index] = newValue;
        }

        public override void CommitValue()
        {
            if (_isReadOnly || !IsTargetValid)
            {
                return;
            }

            string[] captured = (string[])_components.Clone();

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Refresh any cached copy of the containing value before reading, so the
                    // components we don't touch are carried over from the current value.
                    PreWriteCallback?.Invoke();

                    TStruct updated = _readStruct();

                    for (int i = 0; i < _componentCount; i++)
                    {
                        if (TryParseComponent(captured[i], out float parsed))
                        {
                            updated = _withComponent(updated, i, parsed);
                        }
                    }

                    if (_writeStruct != null)
                    {
                        _writeStruct(updated);

                        Dispatcher.UIThread.Post(() =>
                        {
                            RefreshValue();
                            ValueChangedCallback?.Invoke();
                        });
                    }
                    else
                    {
                        using BoxedValue boxed = new BoxedValue(updated);
                        CommitPropertyChange($"Set {Label}", boxed);
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to write vector property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
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

        // Sim thread only.
        private TStruct ReadStructFromProperty()
        {
            using BoxedValue boxed = GetPropertyValue();

            IntPtr ptr = boxed.Pointer;

            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException($"Property '{Label}' returned null pointer");
            }

            return Marshal.PtrToStructure<TStruct>(ptr);
        }
    }
}
