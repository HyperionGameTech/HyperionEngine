#include <asset/serialization/fbom/FBOMNameTable.hpp>
#include <asset/serialization/fbom/FBOMWriter.hpp>

namespace hyperion::fbom {

FBOMResult FBOMNameTable::Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes) const
{
    return writer->Write(out, *this, id, attributes);
}

} // namespace hyperion::fbom