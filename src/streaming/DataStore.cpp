#include <streaming/DataStore.hpp>
#include <asset/ByteWriter.hpp>

#include <asset/Assets.hpp>

namespace hyperion::v2 {

extern AssetManager *g_asset_manager;

DataStoreBase::DataStoreBase(const String &prefix)
    : m_prefix(prefix)
{
}

bool DataStoreBase::MakeDirectory() const
{
    const FilePath path = GetDirectory();
    DebugLog(LogType::Debug, "Path: %s\n", path.Data());

    if (!path.Exists() || !path.IsDirectory()) {
        return path.MkDir() == 0;
    }

    return true;
}

void DataStoreBase::Write(const String &key, const ByteBuffer &byte_buffer)
{
    const FilePath directory = GetDirectory();

    AssertThrowMsg(MakeDirectory(), "Failed to make directory %s", directory.Data());

    const FilePath filepath = directory / key;

    FileByteWriter writer(filepath.Data());
    writer.Write(byte_buffer.Data(), byte_buffer.Size());
    writer.Close();
}

bool DataStoreBase::Read(const String &key, ByteBuffer &out_byte_buffer) const
{
    const FilePath directory = GetDirectory();

    if (!directory.Exists() || !directory.IsDirectory()) {
        return false;
    }

    const FilePath filepath = directory / key;

    if (!filepath.Exists()) {
        return false;
    }
    
    BufferedByteReader reader(filepath);
    out_byte_buffer = reader.ReadBytes();

    return true;
}

FilePath DataStoreBase::GetDirectory() const
{
    return g_asset_manager->GetBasePath() / "data" / m_prefix;
}

} // namespace hyperion::v2