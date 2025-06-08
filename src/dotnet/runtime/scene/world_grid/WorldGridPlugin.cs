using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name = "WorldGridPlugin")]
    public abstract class WorldGridPlugin : HypObject
    {
        public WorldGridPlugin()
        {
        }

        public abstract StreamingCell CreatePatch(StreamingCellInfo cell_info);
    }
}