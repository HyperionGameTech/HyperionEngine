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

        public bool CanAddElement { get; }
        public bool CanRemoveElement { get; }

        // Arrays span both label and value columns (like structs).
        public override bool ShowInlineLabel => false;


        public ArrayPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;

            TypeInfo propertyTypeInfo = property.TypeInfo;
            _elementTypeInfo = propertyTypeInfo.GetElementTypeInfo();

            Value = propertyTypeInfo.Name.ToString();

            bool isFixedArray = propertyTypeInfo.Name.ToString().Contains("FixedArray");

            CanAddElement = !isReadOnly && !isFixedArray;
            CanRemoveElement = !isReadOnly && !isFixedArray;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;

            TypeInfo propertyTypeInfo = property.TypeInfo;
            _elementTypeInfo = propertyTypeInfo.GetElementTypeInfo();

            Value = propertyTypeInfo.Name.ToString();

            bool isFixedArray = propertyTypeInfo.Name.ToString().Contains("FixedArray");

            CanAddElement = !isReadOnly && !isFixedArray;
            CanRemoveElement = !isReadOnly && !isFixedArray;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(string label, TypeInfo typeInfoHint, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfoHint, getter, setter, isReadOnly)
        {
            _depth = depth;

            _elementTypeInfo = typeInfoHint.GetElementTypeInfo();

            Value = _elementTypeInfo.Name.ToString();

            CanAddElement = !isReadOnly;
            CanRemoveElement = !isReadOnly;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }


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


        public void AddElement()
        {
            if (_depth >= MaxDepth)
                return;

            if (_currentArrayValue == null)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Resize by 1 — the underlying container default-constructs
                    // the new element (null handle, zero float, etc.).
                    int currentSize = _currentArrayValue!.GetArraySize();
                    _currentArrayValue.ResizeArray(currentSize + 1);

                    WriteArrayToParent();

                    // Re-read the array to get the canonical copy from the engine.
                    BoxedValue refreshed = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _currentArrayValue = refreshed;

                        // Full rebuild — keeps indices and labels consistent.
                        RebuildElementVMs(_currentArrayValue);

                        foreach (var vm in Elements)
                        {
                            vm.RefreshValue();
                        }

                        HasElements = Elements.Count > 0;
                        Value = $"(array, {Elements.Count} elem{(Elements.Count != 1 ? "s" : "")})";
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
            if (_currentArrayValue == null)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _currentArrayValue!.RemoveArrayElement(index);
                    WriteArrayToParent();

                    BoxedValue refreshed = GetPropertyValue();

                    Dispatcher.UIThread.Post(() =>
                    {
                        _currentArrayValue = refreshed;

                        RebuildElementVMs(_currentArrayValue);

                        foreach (var vm in Elements)
                        {
                            vm.RefreshValue();
                        }

                        HasElements = Elements.Count > 0;
                        Value = $"(array, {Elements.Count} elem{(Elements.Count != 1 ? "s" : "")})";
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
