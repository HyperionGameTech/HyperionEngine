using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="StreamedDataBase")]
    public class StreamedDataBase : HypObject
    {
        public StreamedDataBase() : base()
        {
        }
    }

    [HypClassBinding(Name="NullStreamedData")]
    public class NullStreamedData : StreamedDataBase
    {
        public NullStreamedData() : base()
        {
        }
    }

    [HypClassBinding(Name="MemoryStreamedData")]
    public class MemoryStreamedData : StreamedDataBase
    {
        public MemoryStreamedData() : base()
        {
        }
    }
}