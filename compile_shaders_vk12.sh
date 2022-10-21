#!/bin/bash

pushd res/vkshaders || exit

VERSION_TYPE="--target-spv=spv1.5"
glslc $VERSION_TYPE deferred_direct.frag -o deferred_direct.frag.spv
glslc $VERSION_TYPE deferred_indirect.frag -o deferred_indirect.frag.spv
glslc $VERSION_TYPE deferred.vert -o deferred.vert.spv
glslc $VERSION_TYPE blit.frag -o blit_frag.spv
glslc $VERSION_TYPE blit.vert -o blit_vert.spv
glslc $VERSION_TYPE forward.frag -o forward_frag.spv
glslc $VERSION_TYPE Terrain.frag -o Terrain.frag.spv
glslc $VERSION_TYPE main.vert -o vert.spv
glslc $VERSION_TYPE cubemap_renderer.frag -o cubemap_renderer.frag.spv
glslc $VERSION_TYPE cubemap_renderer.vert -o cubemap_renderer.vert.spv

glslc $VERSION_TYPE imagestore.comp -o imagestore.comp.spv

glslc $VERSION_TYPE cull/generate_depth_pyramid.comp -o cull/generate_depth_pyramid.comp.spv
glslc $VERSION_TYPE cull/object_visibility.comp -o cull/object_visibility.comp.spv

glslc $VERSION_TYPE SSAO.frag -o SSAO.frag.spv
glslc $VERSION_TYPE PostEffect.vert -o PostEffect.vert.spv
glslc $VERSION_TYPE fxaa.frag -o fxaa.frag.spv
glslc $VERSION_TYPE tonemap.frag -o tonemap.frag.spv

glslc $VERSION_TYPE ssr/ssr_write_uvs.comp -o ssr/ssr_write_uvs.comp.spv
glslc $VERSION_TYPE ssr/ssr_sample.comp -o ssr/ssr_sample.comp.spv
glslc $VERSION_TYPE ssr/ssr_blur_hor.comp -o ssr/ssr_blur_hor.comp.spv
glslc $VERSION_TYPE ssr/ssr_blur_vert.comp -o ssr/ssr_blur_vert.comp.spv

glslc $VERSION_TYPE aabb.frag -o aabb.frag.spv
glslc $VERSION_TYPE aabb.vert -o aabb.vert.spv

glslc $VERSION_TYPE skybox.frag -o skybox_frag.spv
glslc $VERSION_TYPE skybox.vert -o skybox_vert.spv

glslc $VERSION_TYPE shadow.frag -o shadow_frag.spv
glslc $VERSION_TYPE shadow.vert -o shadow_vert.spv
glslc $VERSION_TYPE shadow/BlurShadowMap.comp -o shadow/BlurShadowMap.comp.spv

glslc $VERSION_TYPE outline.frag -o outline.frag.spv
glslc $VERSION_TYPE outline.vert -o outline.vert.spv

glslc $VERSION_TYPE vegetation.vert -o vegetation.vert.spv

glslc $VERSION_TYPE voxel/octree_alloc_nodes.comp -o voxel/octree_alloc_nodes.comp.spv
glslc $VERSION_TYPE voxel/octree_init_nodes.comp -o voxel/octree_init_nodes.comp.spv
glslc $VERSION_TYPE voxel/octree_modify_args.comp -o voxel/octree_modify_args.comp.spv
glslc $VERSION_TYPE voxel/octree_tag_nodes.comp -o voxel/octree_tag_nodes.comp.spv
glslc $VERSION_TYPE voxel/octree_write_mipmaps.comp -o voxel/octree_write_mipmaps.comp.spv
glslc $VERSION_TYPE voxel/voxelize.frag -o voxel/voxelize.frag.spv
glslc $VERSION_TYPE voxel/voxelize.geom -o voxel/voxelize.geom.spv
glslc $VERSION_TYPE voxel/voxelize.vert -o voxel/voxelize.vert.spv

glslc $VERSION_TYPE vct/voxelize.frag -o vct/voxelize.frag.spv
glslc $VERSION_TYPE vct/voxelize.geom -o vct/voxelize.geom.spv
glslc $VERSION_TYPE vct/voxelize.vert -o vct/voxelize.vert.spv
glslc $VERSION_TYPE vct/clear_voxels.comp -o vct/clear_voxels.comp.spv
glslc $VERSION_TYPE vct/GenerateMipmap.comp -o vct/GenerateMipmap.comp.spv
glslc $VERSION_TYPE vct/TemporalBlending.comp -o vct/TemporalBlending.comp.spv

glslc $VERSION_TYPE particles/UpdateParticles.comp -o particles/UpdateParticles.comp.spv
glslc $VERSION_TYPE particles/Particle.frag -o particles/Particle.frag.spv
glslc $VERSION_TYPE particles/Particle.vert -o particles/Particle.vert.spv

glslc $VERSION_TYPE deferred/DeferredCombine.comp -o deferred/DeferredCombine.comp.spv

glslc $VERSION_TYPE ui/UIObject.frag -o ui/UIObject.frag.spv
glslc $VERSION_TYPE ui/UIObject.vert -o ui/UIObject.vert.spv

popd
