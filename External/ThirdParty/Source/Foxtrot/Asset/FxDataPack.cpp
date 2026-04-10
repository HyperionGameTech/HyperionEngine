#include "FxDataPack.hpp"

#include <Asset/AxPaths.hpp>
#include <Core/FxFile.hpp>
#include <Core/FxHash.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <Math/FxMathUtil.hpp>

static const uint16 scBinHeaderStart = 0xA1B2;
static const uint16 scBinHeaderEnd = 0x2B1A;

/*
 * HEADER BEGIN - A1B2
 *      ENTRY 1    (16 bytes)
 *      - ENTRY ID (8 bytes)
 *      - OFFSET   (4 bytes)
 *      - SIZE     (4 bytes)
 *      ENTRY 2    (16 bytes)
 *      ENTRY N
 * HEADER END   - 2B1A
 * ENTRY 1 DATA
 * ENTRY 2 DATA
 * ENTRY N DATA
 */


// static constexpr uint16 scByteAlignment = 16;
// static constexpr uint8 scEntryStart[2] = { 0xFE, 0xED };

// void FxDataPack::AddEntry(const FxSlice<uint8>& identifier, const FxSlice<uint8>& data)
// {
//     AddEntry(FxHashData64(FxMakeSlice<uint8>(identifier.pData, identifier.Size)), data);
// }

void FxDataPack::AddEntry(FxHash64 id, const FxSlice<uint8>& data)
{
    if (!Entries.IsInited()) {
        Entries.Create(16);
    }

    // FxLogInfo("SAVE ({}), {:.{}}", static_cast<uint32>(data.Size), reinterpret_cast<const char*>(data.pData),
    //           static_cast<uint32>(data.Size));

    // Check to see if the entry exists and update it if it does
    if (!Entries.IsEmpty()) {
        FxDataPackEntry* found_entry = nullptr;
        for (FxDataPackEntry& entry : Entries) {
            if (entry.Id == id) {
                found_entry = &entry;
            }
        }

        if (found_entry) {
            found_entry->Data.Free();
            found_entry->Data.InitAsCopyOf(data.pData, data.Size);
            found_entry->DataSize = data.Size;

            FxLogInfo("Updating data pack entry {}", id);
            return;
        }
    }

    FxSizedArray<uint8> data_arr;
    data_arr.InitAsCopyOf(data.pData, data.Size);

    FxDataPackEntry pack { id, std::move(data_arr) };
    Entries.Insert(std::move(pack));
}


bool FxDataPack::BinaryReadHeader()
{
    uint16 header_start = 0;
    File.Read<uint16>(FxMakeSlice(&header_start, 1));

    uint16 number_of_entries;
    File.Read<uint16>(FxMakeSlice(&number_of_entries, 1));

    if (!Entries.IsInited()) {
        Entries.Create(number_of_entries);
    }

    if (header_start != scBinHeaderStart) {
        return false;
    }

    for (int index = 0; index < number_of_entries; index++) {
        FxHash64 id;
        File.Read<uint64>(FxMakeSlice(&id, 1));

        uint32 offset;
        File.Read<uint32>(FxMakeSlice(&offset, 1));

        uint32 size;
        File.Read<uint32>(FxMakeSlice(&size, 1));


        FxSizedArray<uint8> data;
        data.InitSize(size);

        FxDataPackEntry entry { id, std::move(data), offset, size };
        Entries.Insert(std::move(entry));
    }

    uint16 header_end;
    File.Read<uint16>(FxMakeSlice(&header_end, 1));

    if (header_end != scBinHeaderEnd) {
        return false;
    }

    return true;
}

void FxDataPack::BinaryWriteHeader()
{
    uint32 header_size = 0;
    for (FxDataPackEntry& _ : Entries) {
        constexpr int cSizeOfEntry = sizeof(uint64) + sizeof(uint32) +
                                     sizeof(uint32); // identifier_hash, offset, data_size
        header_size += cSizeOfEntry;
    }

    // Header start(uint16), entry count (uint16), header end(uint16)
    header_size += 6;

    uint32 offset = header_size;

    File.Write(scBinHeaderStart);
    File.Write<uint16>(static_cast<uint16>(Entries.Size()));

    for (FxDataPackEntry& entry : Entries) {
        File.Write<uint64>(entry.Id);
        File.Write<uint32>(offset);
        File.Write<uint32>(entry.Data.Size);

        offset += entry.Data.Size;
    }

    File.Write(scBinHeaderEnd);
}

void FxDataPack::BinaryWriteData()
{
    for (const FxDataPackEntry& entry : Entries) {
        File.Write(entry.Data.pData, entry.Data.Size);
    }
}

void FxDataPack::BinaryReadAllData()
{
    if (Entries.IsEmpty()) {
        BinaryReadHeader();
    }

    FxLogDebug("Reading all data...\n");
    for (FxDataPackEntry& entry : Entries) {
        entry.Data = ReadSection<uint8>(&entry);
    }
}


void FxDataPack::PrintInfo() const
{
    FxLogInfo("");
    FxLogInfo("=== DataPack ===");


    for (const FxDataPackEntry& entry : Entries) {
        FxLogInfo("Entry 0x{:x} => Offset={}, Size={}", entry.Id, entry.DataOffset, entry.DataSize);
    }

    FxLogInfo("================");
    FxLogInfo("");
}

void FxDataPack::JumpToEntry(FxHash64 id)
{
    FxDataPackEntry* found_entry = nullptr;
    for (FxDataPackEntry& entry : Entries) {
        if (entry.Id == id) {
            found_entry = &entry;
        }
    }

    if (!found_entry) {
        FxLogError("Could not find entry {} in FxDataPack", id);
        return;
    }
}

FxDataPackEntry* FxDataPack::QuerySection(FxHash64 id) const
{
    // TODO: replace with ordered map query

    FxDataPackEntry* found_entry = nullptr;
    for (FxDataPackEntry& entry : Entries) {
        if (entry.Id == id) {
            found_entry = &entry;
            break;
        }
    }

    return found_entry;
}

void FxDataPack::WriteToFile(const char* name)
{
    if (File.IsFileOpen()) {
        File.Flush();
        File.Close();
    }

    File.Open(name, FxFile::eWrite, FxFile::eBinary);

    if (!File.IsFileOpen()) {
        FxLogError("Data pack '{}' could not be written to", name);
        return;
    }

    BinaryWriteHeader();
    BinaryWriteData();
}


bool FxDataPack::ReadFromFile(const char* name)
{
    if (File.IsFileOpen()) {
        File.Flush();
        File.Close();
    }
    if (!Entries.IsInited()) {
        Entries.Create(16);
    }
    if (!Entries.IsEmpty()) {
        Entries.Clear();
    }

    File.Open(name, FxFile::eRead, FxFile::eBinary);

    if (!File.IsFileOpen()) {
        FxLogError("Error opening data pack '{}'", name);
        return false;
    }

    bool successful = BinaryReadHeader();
    if (!successful) {
        File.Close();
        // Open the file in write mode to clear it
        File.Open(name, FxFile::eWrite, FxFile::eBinary);
        File.Close();

        // Return unsuccessful
        return false;
    }

    return true;
}

void FxDataPack::Close()
{
    if (File.IsFileOpen()) {
        File.Close();
    }
}


FxDataPack::~FxDataPack() { Close(); }
