using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace Hyperion.Editor.ViewModels
{
    public class ContentDirectoryViewModel : ViewModelBase
    {
        public ContentDirectoryViewModel(string name, string fullPath, IEnumerable<ContentDirectoryViewModel>? children = null)
        {
            Name = name;
            FullPath = fullPath;

            if (children != null)
            {
                foreach (ContentDirectoryViewModel child in children)
                {
                    Children.Add(child);
                }
            }
        }

        public string Name { get; }
        public string FullPath { get; }

        public ObservableCollection<ContentDirectoryViewModel> Children { get; } = new ObservableCollection<ContentDirectoryViewModel>();

        public IReadOnlyList<ContentDirectoryViewModel> GetOrderedChildren()
        {
            return Children.OrderBy(child => child.Name, StringComparer.OrdinalIgnoreCase).ToList();
        }
    }
}
