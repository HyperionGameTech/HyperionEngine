using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="UIDataSourceBase")]
    public abstract class UIDataSourceBase : HypObject
    {
        public UIDataSourceBase()
        {
        }
    }

    [HypClassBinding(Name="UIDataSource")]
    public class UIDataSource : UIDataSourceBase
    {
        public UIDataSource()
        {
        }
    }
}