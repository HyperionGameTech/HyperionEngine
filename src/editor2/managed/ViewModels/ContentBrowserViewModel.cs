using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ContentBrowserViewModel : ViewModelBase, IDisposable
    {
        private readonly EditorSubsystem _editorSubsystem;

        public ObservableCollection<ContentDirectoryViewModel> Directories { get; } = new ObservableCollection<ContentDirectoryViewModel>();
        public ObservableCollection<ContentItemViewModel> Items { get; } = new ObservableCollection<ContentItemViewModel>();
        public ContentDirectoryViewModel? SelectedDirectory { get; private set; }

        public ContentBrowserViewModel(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));
        }

        public void LoadPackages()
        {
            Dispatcher.UIThread.CheckAccess();

            Directories.Clear();
            Items.Clear();

            AssetManager mgr = AssetManager.Instance;
            AssetRegistry registry = mgr.AssetRegistry;

            
        }

        public void Dispose()
        {
        }
    }
}
