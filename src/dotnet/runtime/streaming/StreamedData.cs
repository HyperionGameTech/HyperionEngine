using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="StreamedData")]
    public class StreamedData : HypObject
    {
        public StreamedData() : base()
        {
        }
    }

    [HypClassBinding(Name="NullStreamedData")]
    public class NullStreamedData : StreamedData
    {
        public NullStreamedData() : base()
        {
        }
    }

    [HypClassBinding(Name="MemoryStreamedData")]
    public class MemoryStreamedData : StreamedData
    {
        public MemoryStreamedData() : base()
        {
        }
    }
}