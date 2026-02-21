
using System;
using System.IO;
using System.Collections.Generic;
using Hyperion;

namespace Hyperion.Editor.UI;

[ClassBinding(IsDynamic = true)]
public unsafe struct FileInfo
{
    private fixed byte name[2048];
    private fixed byte path[4096];
    private bool isDirectory;

    public string Name
    {
        get
        {
            fixed (byte* namePtr = name)
            {
                return new string((sbyte*)namePtr);
            }
        }
        set
        {
            fixed (byte* namePtr = name)
            {
                for (int i = 0; i < 2048; i++)
                {
                    name[i] = 0;
                }

                for (int i = 0; i < value.Length; i++)
                {
                    if (i == 2047)
                    {
                        break;
                    }

                    name[i] = (byte)value[i];
                }
            }
        }
    }

    public string Path
    {
        get
        {
            fixed (byte* pathPtr = path)
            {
                return new string((sbyte*)pathPtr);
            }
        }
        set
        {
            fixed (byte* pathPtr = path)
            {
                for (int i = 0; i < 4096; i++)
                {
                    path[i] = 0;
                }

                for (int i = 0; i < value.Length; i++)
                {
                    if (i == 4095)
                    {
                        break;
                    }

                    path[i] = (byte)value[i];
                }
            }
        }
    }

    public bool IsDirectory
    {
        get
        {
            return isDirectory;
        }
        set
        {
            isDirectory = value;
        }
    }
}

public class FileInfoUIElementFactory : UIElementFactoryBase
{
    public override UIObject CreateUIObject(UIObject parent, object value, object context)
    {
        FileInfo fileInfo = (FileInfo)value;

        UIButton button = parent.Spawn<UIButton>(new Name("Button"));
        button.SetSize(new UIObjectSize(100, UIObjectSizeFlags.Percent, 100, UIObjectSizeFlags.Percent));
        button.SetText(fileInfo.Name);
        button.SetBorderRadius(0);
        button.SetBackgroundColor(new Color(0));

        return button;
    }

    public override void UpdateUIObject(UIObject uiObject, object value, object context)
    {
        FileInfo fileInfo = (FileInfo)value;

        UIButton button = (UIButton)uiObject;
        button.SetText(fileInfo.Name);
    }
}

public class FileBrowserDialog : UIEventHandler
{
    private string currentPath = "";
    private UIGrid? contentsGrid;
    private UIPanel? noDataPanel;
    private UIButton? backButton;
    private Stack<string> pathStack = new Stack<string>();
    private string filter = "*.obj";

    public override void OnAdded(Entity entity)
    {
        base.OnAdded(entity);

        Logger.Log(LogLevel.Info, "FileBrowserDialog added");
    }

    [UIEvent(AllowNested = true)]
    public override UIEventHandlerResult OnAttached()
    {
        Logger.Log(LogLevel.Info, "OnAttached");

        Class cls = Class.GetClass<FileInfo>();

        contentsGrid = UIObject.Find<UIGrid>(new Name("File_Browser_Dialog_Contents", weak: true));
        contentsGrid?.SetIsVisible(false);

        UIDataSource dataSource = new UIDataSource(TypeId.ForType<FileInfo>(), new FileInfoUIElementFactory());
        contentsGrid?.SetDataSource(dataSource);

        noDataPanel = UIObject.Find<UIPanel>(new Name("File_Browser_Dialog_NoData_Panel", weak: true));
        noDataPanel?.SetIsVisible(true);

        backButton = UIObject.Find<UIButton>(new Name("File_Browser_Dialog_Back_Button", weak: true));
        backButton?.SetIsVisible(false);

        PushDirectory(Directory.GetCurrentDirectory());

        return UIEventHandlerResult.Ok;
    }

    [UIEvent]
    public UIEventHandlerResult BackButtonClicked()
    {
        Logger.Log(LogLevel.Info, "Back button clicked");

        PopDirectory();

        return UIEventHandlerResult.Ok;
    }

    private void PushDirectory(string path)
    {
        pathStack.Push(currentPath);
        currentPath = path;
        UpdateContents();

        backButton?.SetIsVisible(pathStack.Count > 1);
    }

    private void PopDirectory()
    {
        if (pathStack.Count > 0)
        {
            currentPath = pathStack.Pop();
            UpdateContents();
        }

        backButton?.SetIsVisible(pathStack.Count > 1);
    }

    private void UpdateContents()
    {
        List<FileInfo> files = GetFiles(currentPath);

        UIDataSourceBase? dataSource = contentsGrid?.GetDataSource();
        Assert.Throw(dataSource != null, "DataSource is null");

        dataSource.Clear();

        foreach (FileInfo fileInfo in files)
        {
            dataSource.Push(new UUID(), fileInfo);
        }

        if (dataSource.Size() == 0)
        {
            contentsGrid?.SetIsVisible(false);
            noDataPanel?.SetIsVisible(true);
        }
        else
        {
            contentsGrid?.SetIsVisible(true);
            noDataPanel?.SetIsVisible(false);
        }
    }

    private List<FileInfo> GetFiles(string path)
    {
        List<FileInfo> files = new List<FileInfo>();

        try
        {
            foreach (string directory in Directory.GetDirectories(path))
            {
                files.Add(new FileInfo
                {
                    Name = Path.GetFileName(directory),
                    Path = directory,
                    IsDirectory = true
                });
            }

            // Files
            string[] filters = filter.Split(';', StringSplitOptions.RemoveEmptyEntries);
            foreach (string file in Directory.GetFiles(path))
            {
                string fileExtension = Path.GetExtension(file).ToLowerInvariant();
                bool matchesFilter = false;

                foreach (string f in filters)
                {
                    string filterExtension = Path.GetExtension(f).ToLowerInvariant();
                    if (filterExtension.Equals(fileExtension, StringComparison.OrdinalIgnoreCase))
                    {
                        matchesFilter = true;
                        break;
                    }
                }

                if (!matchesFilter)
                {
                    continue;
                }

                files.Add(new FileInfo
                {
                    Name = Path.GetFileName(file),
                    Path = file,
                    IsDirectory = false
                });
            }
        }
        catch (Exception e)
        {
            Logger.Log(LogLevel.Error, $"Error getting files: {e.Message}");
        }

        return files;
    }
}