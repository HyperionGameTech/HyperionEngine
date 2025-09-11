using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum EngineStatEntryType
    {
        Float,
        Double,
        Int,
        UInt,
        Long,
        ULong
    }

    [HypClassBinding(Name = "EngineStatEntry")]
    [StructLayout(LayoutKind.Explicit, Size = 24)]
    public struct EngineStatEntry
    {
        [FieldOffset(0)]
        public Name name;

        [FieldOffset(8)]
        public float valueFloat;

        [FieldOffset(8)]
        public double valueDouble;

        [FieldOffset(8)]
        public int valueInt;

        [FieldOffset(8)]
        public uint valueUInt;

        [FieldOffset(8)]
        public long valueLong;

        [FieldOffset(8)]
        public ulong valueULong;

        [FieldOffset(16)]
        public EngineStatEntryType type;

        public EngineStatEntry(Name statName)
        {
            name = statName;
            valueFloat = 0.0f;
            type = EngineStatEntryType.Float; // Default type
        }

        public EngineStatEntry(Name statName, float value)
        {
            name = statName;
            valueFloat = value;
            type = EngineStatEntryType.Float;
        }

        public EngineStatEntry(Name statName, double value)
        {
            name = statName;
            valueDouble = value;
            type = EngineStatEntryType.Double;
        }

        public EngineStatEntry(Name statName, int value)
        {
            name = statName;
            valueInt = value;
            type = EngineStatEntryType.Int;
        }

        public EngineStatEntry(Name statName, uint value)
        {
            name = statName;
            valueUInt = value;
            type = EngineStatEntryType.UInt;
        }

        public EngineStatEntry(Name statName, long value)
        {
            name = statName;
            valueLong = value;
            type = EngineStatEntryType.Long;
        }

        public EngineStatEntry(Name statName, ulong value)
        {
            name = statName;
            valueULong = value;
            type = EngineStatEntryType.ULong;
        }
    }

    [HypClassBinding(Name = "EngineStatGroup")]
    public class EngineStatGroup : HypObject
    {
        public EngineStatGroup()
        {
            // Initialize the stat group
        }

        public IEnumerable<EngineStatEntry> Entries
        {
            get
            {
                HypField? field = HypClass.GetField(new Name("entries", weak: true));
                Assert.Throw(field != null, "EngineStatGroup does not have an 'Entries' field");

                IntPtr entriesAddress = NativeAddress + ((IntPtr)((HypField)field).Offset);

                uint count = (uint)ReadNativeField(new Name("count", weak: true));
                Assert.Throw(count <= 32, "incorrect stat group count, would cause out of bounds access");

                for (int i = 0; i < count; i++)
                {
                    IntPtr entryAddress = entriesAddress + (i * Marshal.SizeOf<EngineStatEntry>());
                    EngineStatEntry entry = Marshal.PtrToStructure<EngineStatEntry>(entryAddress);

                    yield return entry;
                }
            }
        }
    }

    [HypClassBinding(Name = "EngineStats")]
    public class EngineStats : HypObject
    {
        public EngineStats()
        {
        }
    }
}