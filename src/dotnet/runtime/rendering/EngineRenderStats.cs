using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum EngineRenderStatsCountType : uint
    {
        DrawCalls = 0,
        InstancedDrawCalls,
        Triangles,
        RenderGroups,
        Views,
        Scenes,
        Lights,
        LightmapVolumes,
        EnvProbes,
        EnvGrids,
        Count
    }

    [StructLayout(LayoutKind.Explicit)]
    public struct EngineRenderStatsCounts
    {
        [FieldOffset(0), MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public uint[] counts;

        public uint this[EngineRenderStatsCountType type]
        {
            get => counts[(int)type];
            set => counts[(int)type] = value;
        }
    }

    [HypClassBinding(Name="EngineRenderStats")]
    [StructLayout(LayoutKind.Sequential)]
    public struct EngineRenderStats
    {
        public double framesPerSecond;
        public double millisecondsPerFrame;
        public double millisecondsPerFrameAvg;
        public double millisecondsPerFrameMax;
        public double millisecondsPerFrameMin;
        public EngineRenderStatsCounts counts;
    }
}