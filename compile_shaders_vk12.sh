#!/bin/bash

pushd res/vkshaders || exit

# deprecated due to new ShaderCompiler

VULKAN_TARGET="vulkan1.2"
glslc --target-env=$VULKAN_TARGET deferred_direct.frag -o deferred_direct.frag.spv
glslc --target-env=$VULKAN_TARGET deferred_indirect.frag -o deferred_indirect.frag.spv
glslc --target-env=$VULKAN_TARGET deferred.vert -o deferred.vert.spv
glslc --target-env=$VULKAN_TARGET blit.frag -o blit_frag.spv
glslc --target-env=$VULKAN_TARGET blit.vert -o blit_vert.spv
glslc --target-env=$VULKAN_TARGET forward.frag -o forward_frag.spv
glslc --target-env=$VULKAN_TARGET Terrain.frag -o Terrain.frag.spv
glslc --target-env=$VULKAN_TARGET main.vert -o vert.spv
glslc --target-env=$VULKAN_TARGET cubemap_renderer.frag -o cubemap_renderer.frag.spv
glslc --target-env=$VULKAN_TARGET cubemap_renderer.vert -o cubemap_renderer.vert.spv

glslc $VERSION_TYPE imagestore.comp -o imagestore.comp.spv

glslc $VERSION_TYPE cull/generate_depth_pyramid.comp -o cull/generate_depth_pyramid.comp.spv
glslc $VERSION_TYPE cull/object_visibility.comp -o cull/object_visibility.comp.spv

glslc $VERSION_TYPE SSAO.frag -o SSAO.frag.spv
glslc $VERSION_TYPE PostEffect.vert -o PostEffect.vert.spv
glslc $VERSION_TYPE fxaa.frag -o fxaa.frag.spv
glslc $VERSION_TYPE tonemap.frag -o tonemap.frag.spv

glslc --target-env=$VULKAN_TARGET hbao/HBAO.comp -o hbao/HBAO.comp.spv

glslc --target-env=$VULKAN_TARGET ssr/ssr_write_uvs.comp -o ssr/ssr_write_uvs.comp.spv
glslc --target-env=$VULKAN_TARGET ssr/ssr_sample.comp -o ssr/ssr_sample.comp.spv
glslc --target-env=$VULKAN_TARGET ssr/ssr_blur_hor.comp -o ssr/ssr_blur_hor.comp.spv
glslc --target-env=$VULKAN_TARGET ssr/ssr_blur_vert.comp -o ssr/ssr_blur_vert.comp.spv

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

glslc --target-env=$VULKAN_TARGET rt/blur/BlurRadianceHor.comp -o rt/blur/BlurRadianceHor.comp.spv
glslc --target-env=$VULKAN_TARGET rt/blur/BlurRadianceVert.comp -o rt/blur/BlurRadianceVert.comp.spv

glslc --target-env=$VULKAN_TARGET rt/probe.rgen -o rt/probe.rgen.spv
glslc --target-env=$VULKAN_TARGET rt/probe.rmiss -o rt/probe.rmiss.spv
glslc --target-env=$VULKAN_TARGET rt/probe.rchit -o rt/probe.rchit.spv
glslc --target-env=$VULKAN_TARGET rt/probe_update_irradiance.comp -o rt/probe_update_irradiance.comp.spv
glslc --target-env=$VULKAN_TARGET rt/probe_update_depth.comp -o rt/probe_update_depth.comp.spv
glslc --target-env=$VULKAN_TARGET rt/copy_border_texels_irradiance.comp -o rt/copy_border_texels_irradiance.comp.spv
glslc --target-env=$VULKAN_TARGET rt/copy_border_texels_depth.comp -o rt/copy_border_texels_depth.comp.spv
glslc --target-env=$VULKAN_TARGET rt/gi/gi.rgen -o rt/gi/gi.rgen.spv
glslc --target-env=$VULKAN_TARGET rt/gi/gi.rmiss -o rt/gi/gi.rmiss.spv
glslc --target-env=$VULKAN_TARGET rt/gi/gi.rchit -o rt/gi/gi.rchit.spv


glslc $VERSION_TYPE ui/UIObject.frag -o ui/UIObject.frag.spv
glslc $VERSION_TYPE ui/UIObject.vert -o ui/UIObject.vert.spv

popd
