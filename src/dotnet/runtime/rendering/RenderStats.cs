using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum RenderStatsCountType : uint
    {
        DrawCalls = 0,
        InstancedDrawCalls,
        Triangles,
        RenderGroups,
        Views,
        Textures,
        Materials,
        Lights,
        LightmapVolumes,
        EnvProbes,
        EnvGrids,
        Count
    }

    [StructLayout(LayoutKind.Explicit)]
    public struct RenderStatsCounts
    {
        [FieldOffset(0), MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public uint[] counts;

        public uint this[RenderStatsCountType type]
        {
            get => counts[(int)type];
            set => counts[(int)type] = value;
        }
    }

    [HypClassBinding(Name="RenderStats")]
    [StructLayout(LayoutKind.Sequential)]
    public struct RenderStats
    {
        public double framesPerSecond;
        public double millisecondsPerFrame;
        public double millisecondsPerFrameAvg;
        public double millisecondsPerFrameMax;
        public double millisecondsPerFrameMin;
        public uint deletionQueueNumElements;
        public uint deletionQueueTotalBytes;
        public RenderStatsCounts counts;
    }
}