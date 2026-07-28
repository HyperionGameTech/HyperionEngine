using System;
using System.Collections.ObjectModel;
using System.Runtime.InteropServices;
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
        private readonly bool _isPolymorphic;

        // Sim-thread-owned copy of the current array value.
        private BoxedValue? _currentArrayValue;

        // Virtual element not yet committed to the real array.
        private ObjectPropertyViewModel? _pendingElement;

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

        public override bool ShowInlineLabel => false;


        public ArrayPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();

            Value = property.TypeInfo.Name.ToString();

            bool isFixedArray = property.TypeInfo.Name.ToString().Contains("FixedArray");
            CanAddElement = !isReadOnly && !isFixedArray;
            CanRemoveElement = !isReadOnly && !isFixedArray;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();

            Value = property.TypeInfo.Name.ToString();

            bool isFixedArray = property.TypeInfo.Name.ToString().Contains("FixedArray");
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
            _isPolymorphic = DetectPolymorphic();

            Value = _elementTypeInfo.Name.ToString();

            CanAddElement = !isReadOnly;
            CanRemoveElement = !isReadOnly;

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }


        private bool DetectPolymorphic()
        {
            Class? elementClass = _elementTypeInfo.Class;
            if (elementClass == null)
                return false;

            string className = elementClass.Value.Name.ToString();
            bool found = false;
            NameCallbackDelegate cb = (_, _) => found = true;
            NativeBindings.Hyp_GetAllDerivedClassNames(className, cb, IntPtr.Zero);
            return found;
        }

        // Replaces the cached copy and disposes the previous one on the calling (sim) thread, rather
        // than leaving engine handles for the finalizer to release off-thread.
        private void ReplaceArrayValue(BoxedValue? newValue)
        {
            BoxedValue? previous = Interlocked.Exchange(ref _currentArrayValue, newValue);

            if (previous != null && !ReferenceEquals(previous, newValue))
            {
                previous.Dispose();
            }
        }

        private BoxedValue RequireArrayValue()
        {
            return Volatile.Read(ref _currentArrayValue)
                ?? throw new InvalidOperationException($"Array value for '{Label}' not yet loaded");
        }

        private BoxedValue GetElementValue(int index) => RequireArrayValue().GetArrayElement(index);

        private void SetElementValue(int index, BoxedValue value) => RequireArrayValue().SetArrayElement(index, value);

        /// <summary>
        /// Sim thread. Re-reads the array from the parent before an element write so the write is
        /// applied on top of the array's current contents, not a stale snapshot.
        /// </summary>
        private void ReloadArrayFromParent()
        {
            PreWriteCallback?.Invoke();

            try
            {
                ReplaceArrayValue(GetPropertyValue());
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: failed to re-read array '{Label}': {ex.Message}");
            }
        }

        private void WriteArrayToParent()
        {
            BoxedValue? current = Volatile.Read(ref _currentArrayValue);

            if (current == null)
                return;

            SetPropertyValue(current);
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
                preWriteCallback: ReloadArrayFromParent,
                postWriteCallback: WriteArrayToParent,
                valueChangedCallback: () => ValueChangedCallback?.Invoke());
        }


        public void AddElement()
        {
            if (_depth >= MaxDepth || _isReadOnly)
                return;

            if (_isPolymorphic)
            {
                if (_pendingElement != null)
                    return;

                int nextIndex = Elements.Count;

                var vm = new ObjectPropertyViewModel(
                    $"[{nextIndex}]",
                    _elementTypeInfo,
                    getter: () => throw new InvalidOperationException("Pending element"),
                    setter: _ => { },
                    isReadOnly: _isReadOnly,
                    depth: _depth + 1);

                vm.IsPending = true;
                vm.OnPendingCommitted = CommitPendingElement;
                _pendingElement = vm;
                Elements.Add(vm);

                HasElements = Elements.Count > 0;
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    ReloadArrayFromParent();

                    BoxedValue current = RequireArrayValue();
                    current.ResizeArray(current.GetArraySize() + 1);

                    WriteArrayToParent();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: AddElement failed: {ex.Message}");
                }

                Dispatcher.UIThread.Post(() =>
                {
                    RefreshValue();
                    ValueChangedCallback?.Invoke();
                });
            });
        }


        private void CommitPendingElement(string className)
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Create the instance.
                    BoxedValueInternal result;

                    unsafe
                    {
                        if (!Hyp_CreateInstanceOfClass(className, &result))
                        {
                            Logger.Log(LogLevel.Warning, $"Failed to create instance of '{className}'");
                            return;
                        }
                    }

                    ReloadArrayFromParent();

                    using (BoxedValue instance = BoxedValue.FromBuffer(result))
                    {
                        RequireArrayValue().PushBackArrayElement(instance);
                    }

                    WriteArrayToParent();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"CommitPendingElement('{className}') failed: {ex.Message}");
                }

                Dispatcher.UIThread.Post(() =>
                {
                    _pendingElement = null;

                    RefreshValue();
                    ValueChangedCallback?.Invoke();
                });
            });
        }


        public void RemoveElementAt(int index)
        {
            if (index < 0 || index >= Elements.Count || _isReadOnly)
                return;

            // Removing the virtual element: just drop it, no array work.
            if (_pendingElement != null && Elements[index] == _pendingElement)
            {
                Elements.RemoveAt(index);
                _pendingElement = null;
                HasElements = Elements.Count > 0;
                return;
            }

            int capturedIndex = index;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    ReloadArrayFromParent();

                    RequireArrayValue().RemoveArrayElement(capturedIndex);

                    WriteArrayToParent();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: RemoveElementAt({capturedIndex}) failed: {ex.Message}");
                }

                Dispatcher.UIThread.Post(() =>
                {
                    RefreshValue();
                    ValueChangedCallback?.Invoke();
                });
            });
        }


        private void RebuildElementVMs(int count)
        {
            Elements.Clear();

            for (int i = 0; i < count; i++)
            {
                Elements.Add(CreateElementViewModel(i));
            }

            int displayCount = count;

            // Re-attach the pending element if it still exists.
            if (_pendingElement != null)
            {
                Elements.Add(_pendingElement);
                displayCount++; // show it in the summary
            }

            HasElements = Elements.Count > 0;
            Value = $"(array, {displayCount} elem{(displayCount != 1 ? "s" : "")})";
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                int count;

                try
                {
                    BoxedValue newArrayValue = GetPropertyValue();
                    ReplaceArrayValue(newArrayValue);

                    count = _depth < MaxDepth ? newArrayValue.GetArraySize() : 0;
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: RefreshValue failed: {ex.Message}");

                    EndRefresh();

                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        // Element view models are bound to an index, so they only need rebuilding
                        // when the element count changes. Rebuilding on every refresh would drop
                        // any expanded/edited state in nested editors.
                        int existingCount = _pendingElement != null ? Elements.Count - 1 : Elements.Count;

                        if (existingCount != count)
                        {
                            RebuildElementVMs(count);
                        }

                        foreach (InspectorPropertyViewModelBase vm in Elements)
                        {
                            vm.RefreshValue();
                        }
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        public override void CommitValue()
        {
        }


        [DllImport("hyperion")]
        private static extern unsafe bool Hyp_CreateInstanceOfClass(string className, BoxedValueInternal* pOutBoxed);
    }
}
