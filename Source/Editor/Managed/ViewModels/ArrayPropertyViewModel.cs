using System;
using System.Collections.Generic;
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

        // Array<BoxedValue>: each element row is built for the type the element actually holds.
        private readonly bool _isTypeErased;

        // Sim-thread-owned copy of the current array value.
        private BoxedValue? _currentArrayValue;

        // The same, per multi-selection peer (indexed like Peers).
        private BoxedValue?[] _peerArrayValues = Array.Empty<BoxedValue?>();

        // Set when the element editor can't edit several objects at once, so no elements are shown.
        private volatile bool _cannotEditElementsTogether;

        // Virtual element not yet committed to the real array.
        private ObjectPropertyViewModel? _pendingElement;

        // UI thread. The type each committed element row was built for.
        private List<TypeInfo> _elementRowTypes = new();

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

        // Both come from the array itself on refresh, so fixed-size arrays never offer them.
        private bool _canAddElement;
        public bool CanAddElement
        {
            get => _canAddElement;
            private set => SetProperty(ref _canAddElement, value);
        }

        private bool _canRemoveElement;
        public bool CanRemoveElement
        {
            get => _canRemoveElement;
            private set => SetProperty(ref _canRemoveElement, value);
        }

        public override bool ShowInlineLabel => false;

        public override bool ShowsDescriptionInOwnTemplate => true;


        public ArrayPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();
            _isTypeErased = IsTypeErasedElementType(_elementTypeInfo);

            Value = property.TypeInfo.Name.ToString();

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = property.TypeInfo.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();
            _isTypeErased = IsTypeErasedElementType(_elementTypeInfo);

            Value = property.TypeInfo.Name.ToString();

            AddElementCommand = new RelayCommand(AddElement, () => !_isReadOnly);
            RemoveElementCommand = new RelayCommand<InspectorPropertyViewModelBase>(vm => RemoveElementAt(Elements.IndexOf(vm!)));
        }

        public ArrayPropertyViewModel(string label, TypeInfo typeInfoHint, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfoHint, getter, setter, isReadOnly)
        {
            _depth = depth;
            _elementTypeInfo = typeInfoHint.GetElementTypeInfo();
            _isPolymorphic = DetectPolymorphic();
            _isTypeErased = IsTypeErasedElementType(_elementTypeInfo);

            Value = _elementTypeInfo.Name.ToString();

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

        private static bool IsTypeErasedElementType(TypeInfo typeInfo)
        {
            return !typeInfo.IsNull && typeInfo.Name == "Hyperion::BoxedValue";
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

            if (TryWriteContainerValueToSwatchOverride(current))
            {
                PostWriteCallback?.Invoke();
            }
            else
            {
                SetPropertyValue(current);
            }
        }

        private void ReplacePeerArrayValue(int peerIndex, BoxedValue? newValue)
        {
            BoxedValue? previous = Interlocked.Exchange(ref _peerArrayValues[peerIndex], newValue);

            if (previous != null && !ReferenceEquals(previous, newValue))
            {
                previous.Dispose();
            }
        }

        private BoxedValue RequirePeerArrayValue(int peerIndex)
        {
            return Volatile.Read(ref _peerArrayValues[peerIndex])
                ?? throw new InvalidOperationException($"Array value for '{Label}' of a selected object not yet loaded");
        }

        private void ReloadPeerArrayFromParent(int peerIndex)
        {
            PropertyTarget peer = Peers[peerIndex];

            peer.PreWrite?.Invoke();

            try
            {
                ReplacePeerArrayValue(peerIndex, peer.Get());
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: failed to re-read array '{Label}' of a selected object: {ex.Message}");
            }
        }

        private void WritePeerArrayToParent(int peerIndex)
        {
            BoxedValue? current = Volatile.Read(ref _peerArrayValues[peerIndex]);

            if (current == null)
                return;

            PropertyTarget peer = Peers[peerIndex];

            if (!TryWritePeerContainerValueToSwatchOverride(peer, current))
            {
                peer.Set(current);
            }

            peer.PostWrite?.Invoke();
        }

        protected override bool OnPeersAttached()
        {
            _peerArrayValues = new BoxedValue?[Peers.Count];

            return true;
        }

        // Sim thread. Applies an in-place change to this row's own array (target 0) and to every peer's
        // (target i + 1), each re-read first and written back after. A target that fails is left alone.
        private void ModifyEveryArray(Action<int, BoxedValue> modify)
        {
            try
            {
                ReloadArrayFromParent();
                modify(0, RequireArrayValue());
                WriteArrayToParent();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: failed to modify array '{Label}': {ex.Message}");
            }

            for (int i = 0; i < Peers.Count; i++)
            {
                try
                {
                    ReloadPeerArrayFromParent(i);
                    modify(i + 1, RequirePeerArrayValue(i));
                    WritePeerArrayToParent(i);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: failed to modify array '{Label}' of a selected object: {ex.Message}");
                }
            }
        }

        /// <summary>
        /// Sim thread. Adds and removes go through the action stack like element edits do: an element
        /// edit's undo is bound to its index, so an unrecorded add/remove would make it land on the wrong element.
        /// </summary>
        private void PushArrayAction(string actionText, Action<int, BoxedValue> execute, Action<int, BoxedValue> revert)
        {
            bool[] applied = new bool[1 + Peers.Count];

            void Apply(Action<int, BoxedValue> modify)
            {
                ModifyEveryArray(modify);

                Dispatcher.UIThread.Post(() =>
                {
                    RefreshValue();
                    ValueChangedCallback?.Invoke();
                });
            }

            EditorAction action = new EditorAction(
                actionText,
                execute: (_, _) => Apply((target, array) =>
                {
                    applied[target] = false;
                    execute(target, array);
                    applied[target] = true;
                }),
                revert: (_, _) => Apply((target, array) =>
                {
                    if (applied[target])
                    {
                        revert(target, array);
                    }
                }));

            EditorProject? project = EngineManager.CurrentProject;

            if (project != null)
            {
                project.ActionStack.PushAction(action);
            }
            else
            {
                action.Execute(null!, null!);
            }
        }


        private InspectorPropertyViewModelBase? CreateElementViewModel(int index, TypeInfo rowType)
        {
            int capturedIndex = index;

            InspectorPropertyViewModelBase vm = InspectorViewModelFactory.CreateForValue(
                $"[{capturedIndex}]",
                rowType,
                getter: () => GetElementValue(capturedIndex),
                setter: v => SetElementValue(capturedIndex, v),
                isReadOnly: _isReadOnly,
                depth: _depth + 1,
                initialize: false,
                preWriteCallback: ReloadArrayFromParent,
                postWriteCallback: WriteArrayToParent,
                valueChangedCallback: () => ValueChangedCallback?.Invoke());

            // Element N edits element N of every selected object's array (only shown when the lengths match).
            List<PropertyTarget> elementPeers = new List<PropertyTarget>(Peers.Count);

            for (int i = 0; i < Peers.Count; i++)
            {
                int peerIndex = i;

                elementPeers.Add(new PropertyTarget(
                    () => RequirePeerArrayValue(peerIndex).GetArrayElement(capturedIndex),
                    v => RequirePeerArrayValue(peerIndex).SetArrayElement(capturedIndex, v),
                    preWrite: () => ReloadPeerArrayFromParent(peerIndex),
                    postWrite: () => WritePeerArrayToParent(peerIndex)));
            }

            return vm.AttachPeers(elementPeers) ? vm : null;
        }


        public void AddElement()
        {
            if (!CanAddElement)
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
                UpdateSummary();
                return;
            }

            _ = EngineManager.PostToSimThread(() => PushArrayAction(
                $"Add element to {Label}",
                execute: (_, array) => array.ResizeArray(array.GetArraySize() + 1),
                revert: (_, array) => RemoveLastElement(array)));
        }


        // UI thread. The pending row is dropped here so the refresh after the add rebuilds the rows,
        // rather than leaving the dead pending row standing in for the new element.
        private void CommitPendingElement(string className)
        {
            if (_pendingElement != null)
            {
                Elements.Remove(_pendingElement);
                _pendingElement = null;

                HasElements = Elements.Count > 0;
            }

            _ = EngineManager.PostToSimThread(() => PushArrayAction(
                $"Add {className} to {Label}",
                execute: (_, array) =>
                {
                    // Each selected object gets its own instance.
                    using BoxedValue instance = CreateInstanceOfClass(className);
                    array.PushBackArrayElement(instance);
                },
                revert: (_, array) => RemoveLastElement(array)));
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
                UpdateSummary();
                return;
            }

            if (!CanRemoveElement)
                return;

            int capturedIndex = index;

            // Each target's removed element, for undo.
            object?[] removedValues = new object?[1 + Peers.Count];
            bool[] hasRemovedValue = new bool[1 + Peers.Count];

            _ = EngineManager.PostToSimThread(() => PushArrayAction(
                $"Remove element from {Label}",
                execute: (target, array) =>
                {
                    hasRemovedValue[target] = TryReadElementValue(array, capturedIndex, out removedValues[target]);
                    array.RemoveArrayElement(capturedIndex);
                },
                revert: (target, array) => InsertElement(array, capturedIndex, removedValues[target], hasRemovedValue[target])));
        }

        private static void RemoveLastElement(BoxedValue array)
        {
            int size = array.GetArraySize();

            if (size > 0)
            {
                array.RemoveArrayElement(size - 1);
            }
        }

        private static bool TryReadElementValue(BoxedValue array, int index, out object? value)
        {
            try
            {
                using BoxedValue element = array.GetArrayElement(index);
                value = element.GetValue();

                return true;
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: element {index} can't be kept for undo, undoing its removal restores a default value: {ex.Message}");
                value = null;

                return false;
            }
        }

        // Sim thread. Shifts the elements from index onwards up by one and puts the value back at index,
        // or a default element when the value couldn't be kept.
        private static void InsertElement(BoxedValue array, int index, object? value, bool hasValue)
        {
            int size = array.GetArraySize();
            int insertIndex = Math.Min(index, size);

            array.ResizeArray(size + 1);

            using BoxedValue defaultElement = array.GetArrayElement(size);

            for (int i = size; i > insertIndex; i--)
            {
                using BoxedValue shifted = array.GetArrayElement(i - 1);
                array.SetArrayElement(i, shifted);
            }

            if (hasValue)
            {
                try
                {
                    using BoxedValue restored = new BoxedValue(value);
                    array.SetArrayElement(insertIndex, restored);

                    return;
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"ArrayPropertyViewModel: couldn't restore element {insertIndex}, using a default value: {ex.Message}");
                }
            }

            array.SetArrayElement(insertIndex, defaultElement);
        }

        private static unsafe BoxedValue CreateInstanceOfClass(string className)
        {
            BoxedValueInternal result;

            if (!Hyp_CreateInstanceOfClass(className, &result))
                throw new InvalidOperationException($"Failed to create instance of '{className}'");

            return BoxedValue.FromBuffer(result);
        }


        private List<TypeInfo> ReadElementRowTypes(BoxedValue array, int count)
        {
            List<TypeInfo> rowTypes = new List<TypeInfo>(count);

            for (int i = 0; i < count; i++)
            {
                if (!_isTypeErased)
                {
                    rowTypes.Add(_elementTypeInfo);
                    continue;
                }

                using BoxedValue element = array.GetArrayElement(i);
                TypeInfo elementType = element.TypeInfo;

                rowTypes.Add(elementType.IsNull ? _elementTypeInfo : elementType);
            }

            return rowTypes;
        }

        private static bool SameRowTypes(List<TypeInfo> first, List<TypeInfo> second)
        {
            if (first.Count != second.Count)
                return false;

            for (int i = 0; i < first.Count; i++)
            {
                if (first[i].Address != second[i].Address)
                    return false;
            }

            return true;
        }

        private void RebuildElementVMs(List<TypeInfo> rowTypes)
        {
            Elements.Clear();
            _elementRowTypes = rowTypes;

            for (int i = 0; i < rowTypes.Count; i++)
            {
                InspectorPropertyViewModelBase? element = CreateElementViewModel(i, rowTypes[i]);

                if (element == null)
                {
                    _cannotEditElementsTogether = true;
                    Elements.Clear();
                    _elementRowTypes = new List<TypeInfo>();
                    break;
                }

                Elements.Add(element);
            }

            // Re-attach the pending element if it still exists.
            if (_pendingElement != null)
            {
                Elements.Add(_pendingElement);
            }

            HasElements = Elements.Count > 0;
        }

        private void UpdateSummary()
        {
            Value = $"(array, {Elements.Count} elem{(Elements.Count != 1 ? "s" : "")})";
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                bool isShared = true;
                bool canResize;
                bool canPushBack;
                List<TypeInfo> rowTypes;

                try
                {
                    BoxedValue newArrayValue = GetPropertyValue();
                    ReplaceArrayValue(newArrayValue);

                    canResize = newArrayValue.CanResizeArray;
                    canPushBack = newArrayValue.CanPushBackArray;

                    int count = _depth < MaxDepth ? newArrayValue.GetArraySize() : 0;

                    IReadOnlyList<PropertyTarget> peers = Peers;
                    List<BoxedValue> peerArrayValues = new List<BoxedValue>(peers.Count);

                    for (int i = 0; i < peers.Count; i++)
                    {
                        BoxedValue peerArrayValue = peers[i].Get();
                        ReplacePeerArrayValue(i, peerArrayValue);
                        peerArrayValues.Add(peerArrayValue);

                        isShared &= peerArrayValue.GetArraySize() == newArrayValue.GetArraySize();
                    }

                    rowTypes = ReadElementRowTypes(newArrayValue, isShared ? count : 0);

                    // Elements are only edited together when every selected object has the same number,
                    // and, for a type-erased array, the same type at each index.
                    if (isShared && _isTypeErased)
                    {
                        foreach (BoxedValue peerArrayValue in peerArrayValues)
                        {
                            isShared &= SameRowTypes(rowTypes, ReadElementRowTypes(peerArrayValue, count));
                        }
                    }

                    if (_cannotEditElementsTogether || !isShared)
                    {
                        isShared = false;
                        rowTypes = new List<TypeInfo>();
                    }
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
                        CanAddElement = !_isReadOnly && _depth < MaxDepth && (_isPolymorphic ? canPushBack : canResize);
                        CanRemoveElement = !_isReadOnly && canResize;

                        // Element view models are bound to an index, so they only need rebuilding when the
                        // rows change. Rebuilding on every refresh would drop expanded/edited state in nested editors.
                        if (!SameRowTypes(_elementRowTypes, rowTypes))
                        {
                            RebuildElementVMs(rowTypes);
                        }

                        bool showsElements = isShared && !_cannotEditElementsTogether;

                        HasMixedValues = !showsElements;

                        if (showsElements)
                        {
                            UpdateSummary();
                        }
                        else
                        {
                            Value = string.Empty;
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
