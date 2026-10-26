using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia;
using Hyperion;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public sealed record ClassPickerEntry(string Name, int Depth, bool IsAbstract);

    public static class ClassPickerEntries
    {
        private static readonly HashSet<string> CreatableEngineAssetTypeNames = new HashSet<string>(StringComparer.Ordinal)
        {
            "Texture",
            "Material",
            "Decal",
            "Prefab",
            "World",
            "Scene",
            "GroundCover",
            "BoxPhysicsShape",
            "SpherePhysicsShape",
            "PlanePhysicsShape",
            "ConvexHullPhysicsShape",
            "CapsulePhysicsShape",
            "HeightFieldPhysicsShape",
            "CompoundPhysicsShape",
        };

        public static List<ClassPickerEntry> BuildDerived(TypeId baseTypeId)
        {
            List<Class> classes = [];
            ClassCallbackDelegate callback = (cls, _) => classes.Add(new Class(cls));

            NativeBindings.Hyp_GetAllDerivedClasses(ref baseTypeId, callback, IntPtr.Zero);

            Dictionary<string, bool> abstractByName = new();
            Dictionary<string, List<string>> children = new();

            string? baseName = null;
            bool isBaseAbstract = true;

            foreach (Class cls in classes)
            {
                string name = cls.Name.ToString();

                if (cls.TypeId == baseTypeId)
                {
                    baseName = name;
                    isBaseAbstract = cls.IsAbstract;
                    continue;
                }

                Class? parent = cls.GetParent();

                if (!parent.HasValue || !abstractByName.TryAdd(name, cls.IsAbstract))
                {
                    continue;
                }

                string parentName = parent.Value.Name.ToString();

                if (!children.TryGetValue(parentName, out List<string>? siblings))
                {
                    siblings = new List<string>();
                    children.Add(parentName, siblings);
                }

                siblings.Add(name);
            }

            List<ClassPickerEntry> entries = new();

            if (baseName == null)
            {
                return entries;
            }

            void Visit(string name, int depth)
            {
                if (!children.TryGetValue(name, out List<string>? names))
                {
                    return;
                }

                names.Sort(StringComparer.OrdinalIgnoreCase);

                foreach (string child in names)
                {
                    entries.Add(new ClassPickerEntry(child, depth, abstractByName[child]));
                    Visit(child, depth + 1);
                }
            }

            if (!isBaseAbstract)
            {
                entries.Add(new ClassPickerEntry(baseName, 0, false));
                Visit(baseName, 1);
            }
            else
            {
                Visit(baseName, 0);
            }

            return entries;
        }

        public static bool CanCreateAssetForType(Class assetClass)
        {
            Class? assetObjectClass = Class.TryGetClass<AssetObject>();

            if (!assetObjectClass.HasValue || (assetClass != assetObjectClass.Value && !assetClass.IsSubclassOf(assetObjectClass.Value)))
            {
                return false;
            }

            if (assetClass.IsAbstract)
            {
                return false;
            }

            string className = assetClass.Name.ToString();

            if (!NativeBindings.Hyp_CanCreateInstanceOfClass(className))
            {
                return false;
            }

            return assetClass.IsUserDefined || CreatableEngineAssetTypeNames.Contains(className) || EditorPluginService.Instance.IsCreatableAssetClass(className);
        }

        public static IReadOnlyList<ClassPickerEntry> BuildCreatableEntries(Class expected)
        {
            if (!expected.IsAbstract)
            {
                // @TODO: should it still show derived types?
                return CanCreateAssetForType(expected)
                    ? [new ClassPickerEntry(expected.Name.ToString(), 0, false)]
                    : [];
            }

            return BuildDerived(expected.TypeId)
                .Where(entry => entry.IsAbstract
                    || (Class.TryGetClass(entry.Name) is { } cls && CanCreateAssetForType(cls)))
                .ToList();
        }
    }

    public class ClassPickerItemViewModel
    {
        public string? ClassName { get; }
        public bool IsSelectable { get; }
        public bool IsCurrent { get; }
        public Thickness Indent { get; }
        public string IconKind { get; }

        public string MatchBefore { get; }
        public string MatchText { get; }
        public string MatchAfter { get; }

        public ClassPickerItemViewModel(
            string? className,
            int depth,
            bool isSelectable,
            bool isCurrent,
            int matchStart = 0,
            int matchLength = 0)
        {
            ClassName = className;
            IsSelectable = isSelectable;
            IsCurrent = isCurrent;
            Indent = new Thickness(depth * 14, 0, 0, 0);
            IconKind = className == null ? "Close" : isSelectable ? "SymbolClass" : "ChevronDown";

            string text = className ?? "None";

            MatchBefore = text.Substring(0, matchStart);
            MatchText = text.Substring(matchStart, matchLength);
            MatchAfter = text.Substring(matchStart + matchLength);
        }
    }

    public class ClassPickerViewModel : ViewModelBase
    {
        private readonly IReadOnlyList<ClassPickerEntry> _entries;
        private readonly string? _currentClassName;
        private readonly bool _allowNone;

        public ObservableCollection<ClassPickerItemViewModel> Items { get; } = new();

        private string _filter = string.Empty;
        public string Filter
        {
            get => _filter;
            set
            {
                if (SetProperty(ref _filter, value ?? string.Empty))
                {
                    Rebuild();
                }
            }
        }

        private ClassPickerItemViewModel? _selectedItem;
        public ClassPickerItemViewModel? SelectedItem
        {
            get => _selectedItem;
            set => SetProperty(ref _selectedItem, value);
        }

        public bool HasNoMatches => Items.Count == 0;

        public ClassPickerViewModel(IReadOnlyList<ClassPickerEntry> entries, string? currentClassName, bool allowNone)
        {
            _entries = entries;
            _currentClassName = string.IsNullOrEmpty(currentClassName) ? null : currentClassName;
            _allowNone = allowNone;

            Rebuild();
        }

        public void MoveSelection(int delta)
        {
            int index = SelectedItem == null ? (delta > 0 ? -1 : Items.Count) : Items.IndexOf(SelectedItem);

            for (int i = index + delta; i >= 0 && i < Items.Count; i += delta)
            {
                if (Items[i].IsSelectable)
                {
                    SelectedItem = Items[i];
                    return;
                }
            }
        }

        private void Rebuild()
        {
            Items.Clear();

            string filter = _filter.Trim();

            if (filter.Length == 0)
            {
                if (_allowNone)
                {
                    Items.Add(new ClassPickerItemViewModel(null, 0, true, _currentClassName == null));
                }

                foreach (ClassPickerEntry entry in _entries)
                {
                    Items.Add(new ClassPickerItemViewModel(entry.Name, entry.Depth, !entry.IsAbstract, entry.Name == _currentClassName));
                }

                SelectedItem = Items.FirstOrDefault(item => item.IsCurrent && item.IsSelectable)
                    ?? Items.FirstOrDefault(item => item.IsSelectable);
            }
            else
            {
                var matches = _entries
                    .Where(entry => !entry.IsAbstract)
                    .Select(entry => (Entry: entry, Index: entry.Name.IndexOf(filter, StringComparison.OrdinalIgnoreCase)))
                    .Where(match => match.Index >= 0)
                    .OrderBy(match => match.Index == 0 ? 0 : 1)
                    .ThenBy(match => match.Entry.Name, StringComparer.OrdinalIgnoreCase);

                foreach (var (entry, index) in matches)
                {
                    Items.Add(new ClassPickerItemViewModel(entry.Name, 0, true, entry.Name == _currentClassName, index, filter.Length));
                }

                SelectedItem = Items.FirstOrDefault();
            }

            OnPropertyChanged(nameof(HasNoMatches));
        }
    }
}
