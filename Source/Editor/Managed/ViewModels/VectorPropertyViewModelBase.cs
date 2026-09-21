using System;
using System.Collections.Generic;
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

        // Where the vector lives within the property's value - the whole value by default, or e.g. a
        // transform's translation. Both run on the sim thread, once per selected object.
        private readonly Func<BoxedValue, TStruct> _extract;
        private readonly Func<BoxedValue, TStruct, object?> _inject;

        private readonly string[] _components;

        protected VectorPropertyViewModelBase(
            ObjectBase target,
            Property property,
            bool isReadOnly,
            int componentCount,
            Func<TStruct, int, float> getComponent,
            Func<TStruct, int, float, TStruct> withComponent,
            Func<BoxedValue, TStruct>? extract = null,
            Func<BoxedValue, TStruct, object?>? inject = null)
            : base(target, property, isReadOnly)
        {
            _componentCount = componentCount;
            _getComponent = getComponent;
            _withComponent = withComponent;
            _extract = extract ?? ReadStruct;
            _inject = inject ?? ((_, vector) => vector);
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
            Func<BoxedValue, TStruct>? extract = null,
            Func<BoxedValue, TStruct, object?>? inject = null)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _componentCount = componentCount;
            _getComponent = getComponent;
            _withComponent = withComponent;
            _extract = extract ?? ReadStruct;
            _inject = inject ?? ((_, vector) => vector);
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
            _extract = ReadStruct;
            _inject = (_, vector) => vector;
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
                List<TStruct> vectors;

                try
                {
                    vectors = ReadAllTargets(_extract);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read vector for '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                // A component the selected objects disagree on is left blank. Compared as displayed,
                // since that's the value an edit writes back.
                string[] formatted = new string[_componentCount];
                bool hasMixedValues = false;

                for (int i = 0; i < _componentCount; i++)
                {
                    formatted[i] = FormatComponent(_getComponent(vectors[0], i));

                    for (int j = 1; j < vectors.Count; j++)
                    {
                        if (FormatComponent(_getComponent(vectors[j], i)) != formatted[i])
                        {
                            formatted[i] = string.Empty;
                            hasMixedValues = true;
                            break;
                        }
                    }
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            Value = BuildDisplayString(formatted);
                            HasMixedValues = hasMixedValues;

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
                    // Blank (multiple values) components are left as each object has them.
                    CommitPropertyChange($"Set {Label}", current =>
                    {
                        TStruct updated = _extract(current);

                        for (int i = 0; i < _componentCount; i++)
                        {
                            if (TryParseComponent(captured[i], out float parsed))
                            {
                                updated = _withComponent(updated, i, parsed);
                            }
                        }

                        return _inject(current, updated);
                    });
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
        private TStruct ReadStruct(BoxedValue boxed)
        {
            IntPtr ptr = boxed.Pointer;

            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException($"Property '{Label}' returned null pointer");
            }

            return Marshal.PtrToStructure<TStruct>(ptr);
        }
    }
}
