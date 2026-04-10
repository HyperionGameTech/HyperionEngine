#pragma once

#include "FxPagedArray.hpp"
//
#include "FxFile.hpp"
#include "FxHash.hpp"

struct FxBasicDbEntry
{
    FxHash64 KeyHash;
    std::string Value;
};


class FxBasicDb
{
private:
public:
    void Open(const char* path);

    void WriteEntry(const FxBasicDbEntry& entry);
    FxBasicDbEntry* FindEntry(FxHash64 key);

    void SaveToFile();

    bool IsOpen() const { return mbIsOpen; }

    void Close();
    ~FxBasicDb() { Close(); }

private:
    void Parse();


private:
    FxFile mInFile;
    FxFile mOutFile;

    char* mpData = nullptr;

    std::string mPath;
    FxPagedArray<FxBasicDbEntry> mEntryMarkers;

    bool mbPendingSaveToFile : 1 = false;
    bool mbIsOpen : 1 = false;
};
