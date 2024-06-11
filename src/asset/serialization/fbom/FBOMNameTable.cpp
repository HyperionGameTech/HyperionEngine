#include <asset/serialization/fbom/FBOMNameTable.hpp>
#include <asset/serialization/fbom/FBOM.hpp>

namespace hyperion::fbom {

FBOMResult FBOMNameTable::Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out) const
{
    return writer->Write(out, *this, id);
}

} // namespace hyperion::fbom