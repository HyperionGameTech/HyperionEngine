using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="AssetManager")]
    public class AssetManager : HypObject
    {
        public AssetManager()
        {
        }

        public string BasePath
        {
            get
            {
                return (string)GetProperty(PropertyNames.BasePath)
                    .Get(this)
                    .GetValue();
            }
        }
    }
}