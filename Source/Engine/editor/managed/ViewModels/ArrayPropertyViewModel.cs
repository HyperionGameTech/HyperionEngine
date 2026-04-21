using System;
using System.Collections.ObjectModel;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ArrayPropertyViewModel : InspectorPropertyViewModelBase
    {
        private const int MaxDepth = 4;

        private readonly int _depth;
        private readonly TypeInfo _elementTypeInfo;

        // Sim-thread copy of the current array value.
        // Sub-element VMs read/write from this copy; WriteArrayToParent() flushes it back.
        private BoxedValue? _currentArrayValue;

        private int _isRefreshing;

        public ICommand AddElementCommand { get; }
        public ICommand RemoveElementCommand { get; }

        public ObservableCollection<InspectorPropertyViewModelBase> Elements { get; } = new();

        private bool _hasElements;
        public bool HasElements
        {
            get => _hasElements;
            private set => SetProperty(ref _hasElements, value);
        }

        private bool _isExpanded = true;
        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        // Arrays span both label and value columns (like structs).
        public override bool ShowInlineLabel => false;

        // ─────────────────── constructors ────────────────────

        public ArrayPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            Value = "(array)";
            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            Value = "(array)";
            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(string label, TypeInfo typeInfoHint, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfoHint, getter, setter, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = typeInfoHint.GetElementTypeInfo();
            Value = "(array)";
            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        // ─────────────────── element VM helpers ──────────────

        private BoxedValue GetElementValue(int index)
        {
            if (_currentArrayValue == null)
                throw new InvalidOperationException("Array value not yet loaded");

            return _currentArrayValue.GetArrayElement(index);
        }

        private void SetElementValue(int index, BoxedValue value)
        {
            if (_currentArrayValue == null)
                throw new InvalidOperationException("Array value not yet loaded");

            _currentArrayValue.SetArrayElement(index, value);
        }

        /// <summary>Flush the in-memory array copy back to the real property.</summary>
        private void WriteArrayToParent()
        {
            if (_currentArrayValue == null)
                return;

            SetPropertyValue(_currentArrayValue);
        }

        // ─────────────────── element factory ─────────────────

        private InspectorPropertyViewModelBase CreateElementViewModel(int index)
        {
            int capturedIndex = index;

            return InspectorViewModelFactory.CreateForValue(
                $"[{capturedIndex}]",
                _elementTypeInfo,
                getter: () => GetElementValue(capturedIndex),
                setter: v => SetElementValue(capturedIndex, v),
                isReadOnly: _isReadOnly,
                depth: _depth + 1,
                initialize: false,
                postWriteCallback: WriteArrayToParent);
        }

        // ─────────────────── add / remove ────────────────────

        public void AddElement()
        {
            if (_depth >= MaxDepth)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    BoxedValue? newElem = _elementTypeInfo.CreateDefaultValue();

                    if (newElem == null)
                    {
                        Logger.Log(LogLevel.Warning, "ArrayPropertyViewModel: Cannot create default element, element type does not support default construction");
                        return;
                    }

                    // Always get a fresh authoritative copy from the engine. never use _currentArrayValue from the sim thread (it is written on the UI thread).
                    BoxedValue current = GetPropertyValue();

                    using (newElem)
                    {
                        current.PushBackArrayElement(newElem);
                    }

                    SetPropertyValue(current);

                    // Re-read to get the canonical engine copy.
                    BoxedValue refreshed = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _currentArrayValue = refreshed;

                        // Always do a full rebuild so that a concurrent RefreshValue that
                        // already ran cannot cause a duplicate element to appear.
                        RebuildElementVMs(_currentArrayValue);

                        foreach (var vm in Elements)
                            vm.RefreshValue();
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: AddElement failed: {ex.Message}");
                }
            });
        }

        public void RemoveElementAt(int index)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Always get a fresh authoritative copy from the engine. never use
                    // _currentArrayValue from the sim thread (it is written on the UI thread).
                    BoxedValue current = GetPropertyValue();

                    int size = current.GetArraySize();

                    if (index < 0 || index >= size)
                        return;

                    // Shift elements left by copying, then resize.
                    for (int i = index; i < size - 1; i++)
                    {
                        using BoxedValue next = current.GetArrayElement(i + 1);
                        current.SetArrayElement(i, next);
                    }

                    current.ResizeArray(size - 1);
                    SetPropertyValue(current);

                    // Re-read canonical copy.
                    BoxedValue refreshed = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _currentArrayValue = refreshed;

                        // Rebuild element VMs from scratch (indices shifted).
                        RebuildElementVMs(_currentArrayValue);

                        foreach (var vm in Elements)
                            vm.RefreshValue();
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: RemoveElementAt({index}) failed: {ex.Message}");
                }
            });
        }


        private void RebuildElementVMs(BoxedValue arrayValue)
        {
            Elements.Clear();

            if (_depth >= MaxDepth)
            {
                HasElements = false;
                return;
            }

            int count = 0;

            try
            {
                count = arrayValue.GetArraySize();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: Failed to get array size: {ex.Message}");
            }

            for (int i = 0; i < count; i++)
            {
                var vm = CreateElementViewModel(i);
                Elements.Add(vm);
            }

            HasElements = Elements.Count > 0;
            Value = $"(array, {count} elem{(count != 1 ? "s" : "")})";
        }

        public override void RefreshValue()
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    BoxedValue newArrayValue = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;
                        _currentArrayValue = newArrayValue;

                        RebuildElementVMs(_currentArrayValue);

                        // Refresh each element VM now that the array copy is ready.
                        foreach (var vm in Elements)
                        {
                            vm.RefreshValue();
                        }
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: RefreshValue failed: {ex.Message}");
                }
            });
        }

        public override void CommitValue()
        {
        }
    }
}
