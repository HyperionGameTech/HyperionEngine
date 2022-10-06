#!/bin/bash

pushd res/vkshaders

VULKAN_TARGET="vulkan1.2"
glslc --target-env=$VULKAN_TARGET deferred_direct.frag -o deferred_direct.frag.spv -g
glslc --target-env=$VULKAN_TARGET deferred_indirect.frag -o deferred_indirect.frag.spv -g
glslc --target-env=$VULKAN_TARGET deferred.vert -o deferred.vert.spv -g
glslc --target-env=$VULKAN_TARGET blit.frag -o blit_frag.spv -g
glslc --target-env=$VULKAN_TARGET blit.vert -o blit_vert.spv -g
glslc --target-env=$VULKAN_TARGET forward.frag -o forward_frag.spv -g
glslc --target-env=$VULKAN_TARGET Terrain.frag -o Terrain.frag.spv -g
glslc --target-env=$VULKAN_TARGET main.vert -o vert.spv -g
glslc --target-env=$VULKAN_TARGET cubemap_renderer.frag -o cubemap_renderer.frag.spv -g
glslc --target-env=$VULKAN_TARGET cubemap_renderer.vert -o cubemap_renderer.vert.spv -g

glslc --target-env=$VULKAN_TARGET imagestore.comp -o imagestore.comp.spv -g

glslc --target-env=$VULKAN_TARGET cull/generate_depth_pyramid.comp -o cull/generate_depth_pyramid.comp.spv -g
glslc --target-env=$VULKAN_TARGET cull/object_visibility.comp -o cull/object_visibility.comp.spv -g

glslc --target-env=$VULKAN_TARGET SSAO.frag -o SSAO.frag.spv -g
glslc --target-env=$VULKAN_TARGET PostEffect.vert -o PostEffect.vert.spv -g
glslc --target-env=$VULKAN_TARGET fxaa.frag -o fxaa.frag.spv -g
glslc --target-env=$VULKAN_TARGET tonemap.frag -o tonemap.frag.spv -g

glslc --target-env=$VULKAN_TARGET ssr/ssr_write_uvs.comp -o ssr/ssr_write_uvs.comp.spv -g
glslc --target-env=$VULKAN_TARGET ssr/ssr_sample.comp -o ssr/ssr_sample.comp.spv -g
glslc --target-env=$VULKAN_TARGET ssr/ssr_blur_hor.comp -o ssr/ssr_blur_hor.comp.spv -g
glslc --target-env=$VULKAN_TARGET ssr/ssr_blur_vert.comp -o ssr/ssr_blur_vert.comp.spv -g

glslc --target-env=$VULKAN_TARGET aabb.frag -o aabb.frag.spv -g
glslc --target-env=$VULKAN_TARGET aabb.vert -o aabb.vert.spv -g

glslc --target-env=$VULKAN_TARGET skybox.frag -o skybox_frag.spv -g
glslc --target-env=$VULKAN_TARGET skybox.vert -o skybox_vert.spv -g

glslc --target-env=$VULKAN_TARGET shadow.frag -o shadow_frag.spv -g
glslc --target-env=$VULKAN_TARGET shadow.vert -o shadow_vert.spv -g
glslc --target-env=$VULKAN_TARGET shadow/BlurShadowMap.comp -o shadow/BlurShadowMap.comp.spv -g

glslc --target-env=$VULKAN_TARGET outline.frag -o outline.frag.spv -g
glslc --target-env=$VULKAN_TARGET outline.vert -o outline.vert.spv -g

glslc --target-env=$VULKAN_TARGET vegetation.vert -o vegetation.vert.spv -g

glslc --target-env=$VULKAN_TARGET voxel/octree_alloc_nodes.comp -o voxel/octree_alloc_nodes.comp.spv -g
glslc --target-env=$VULKAN_TARGET voxel/octree_init_nodes.comp -o voxel/octree_init_nodes.comp.spv -g
glslc --target-env=$VULKAN_TARGET voxel/octree_modify_args.comp -o voxel/octree_modify_args.comp.spv -g
glslc --target-env=$VULKAN_TARGET voxel/octree_tag_nodes.comp -o voxel/octree_tag_nodes.comp.spv -g
glslc --target-env=$VULKAN_TARGET voxel/octree_write_mipmaps.comp -o voxel/octree_write_mipmaps.comp.spv -g
glslc --target-env=$VULKAN_TARGET voxel/voxelize.frag -o voxel/voxelize.frag.spv -g
glslc --target-env=$VULKAN_TARGET voxel/voxelize.geom -o voxel/voxelize.geom.spv -g
glslc --target-env=$VULKAN_TARGET voxel/voxelize.vert -o voxel/voxelize.vert.spv -g

glslc --target-env=$VULKAN_TARGET vct/voxelize.frag -o vct/voxelize.frag.spv -g
glslc --target-env=$VULKAN_TARGET vct/voxelize.geom -o vct/voxelize.geom.spv -g
glslc --target-env=$VULKAN_TARGET vct/voxelize.vert -o vct/voxelize.vert.spv -g
glslc --target-env=$VULKAN_TARGET vct/clear_voxels.comp -o vct/clear_voxels.comp.spv -g
glslc --target-env=$VULKAN_TARGET vct/GenerateMipmap.comp -o vct/GenerateMipmap.comp.spv -g
glslc --target-env=$VULKAN_TARGET vct/TemporalBlending.comp -o vct/TemporalBlending.comp.spv -g

glslc --target-env=$VULKAN_TARGET rt/test.rgen -o rt/test.rgen.spv -g
glslc --target-env=$VULKAN_TARGET rt/test.rmiss -o rt/test.rmiss.spv -g
glslc --target-env=$VULKAN_TARGET rt/test.rchit -o rt/test.rchit.spv -g

glslc --target-env=$VULKAN_TARGET rt/probe.rgen -o rt/probe.rgen.spv -g
glslc --target-env=$VULKAN_TARGET rt/probe.rmiss -o rt/probe.rmiss.spv -g
glslc --target-env=$VULKAN_TARGET rt/probe.rchit -o rt/probe.rchit.spv -g
glslc --target-env=$VULKAN_TARGET rt/probe_update_irradiance.comp -o rt/probe_update_irradiance.comp.spv -g
glslc --target-env=$VULKAN_TARGET rt/probe_update_depth.comp -o rt/probe_update_depth.comp.spv -g

glslc --target-env=$VULKAN_TARGET particles/UpdateParticles.comp -o particles/UpdateParticles.comp.spv -g
glslc --target-env=$VULKAN_TARGET particles/Particle.frag -o particles/Particle.frag.spv -g
glslc --target-env=$VULKAN_TARGET particles/Particle.vert -o particles/Particle.vert.spv -g

glslc --target-env=$VULKAN_TARGET deferred/DeferredCombine.comp -o deferred/DeferredCombine.comp.spv -g

popd
