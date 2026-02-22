struct MeshDescription
{
    uint bindlessIndex; //!< Vertex buffer = bindlessIndex * 2, Index buffer = bindlessIndex * 2 + 1
    uint material_index;
    uint num_indices;
    uint num_vertices;
};