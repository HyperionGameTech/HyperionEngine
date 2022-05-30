#!/bin/bash

pushd res/vkshaders

VULKAN_TARGET="vulkan1.1"
glslc --target-env=$VULKAN_TARGET deferred.frag -o deferred_frag.spv
glslc --target-env=$VULKAN_TARGET deferred.vert -o deferred_vert.spv
glslc --target-env=$VULKAN_TARGET blit.frag -o blit_frag.spv
glslc --target-env=$VULKAN_TARGET blit.vert -o blit_vert.spv
glslc --target-env=$VULKAN_TARGET forward.frag -o forward_frag.spv
glslc --target-env=$VULKAN_TARGET main.vert -o vert.spv
glslc --target-env=$VULKAN_TARGET imagestore.comp -o imagestore.comp.spv

glslc --target-env=$VULKAN_TARGET filter_pass.frag -o filter_pass_frag.spv
glslc --target-env=$VULKAN_TARGET filter_pass.vert -o filter_pass_vert.spv
glslc --target-env=$VULKAN_TARGET fxaa.frag -o fxaa.frag.spv


glslc --target-env=$VULKAN_TARGET skybox.frag -o skybox_frag.spv
glslc --target-env=$VULKAN_TARGET skybox.vert -o skybox_vert.spv

glslc --target-env=$VULKAN_TARGET shadow.frag -o shadow_frag.spv
glslc --target-env=$VULKAN_TARGET shadow.vert -o shadow_vert.spv

glslc --target-env=$VULKAN_TARGET outline.frag -o outline.frag.spv
glslc --target-env=$VULKAN_TARGET outline.vert -o outline.vert.spv

glslc --target-env=$VULKAN_TARGET vegetation.vert -o vegetation.vert.spv

glslc --target-env=$VULKAN_TARGET voxel/octree_alloc_nodes.comp -o voxel/octree_alloc_nodes.comp.spv
glslc --target-env=$VULKAN_TARGET voxel/octree_init_nodes.comp -o voxel/octree_init_nodes.comp.spv
glslc --target-env=$VULKAN_TARGET voxel/octree_modify_args.comp -o voxel/octree_modify_args.comp.spv
glslc --target-env=$VULKAN_TARGET voxel/octree_tag_nodes.comp -o voxel/octree_tag_nodes.comp.spv
glslc --target-env=$VULKAN_TARGET voxel/octree_write_mipmaps.comp -o voxel/octree_write_mipmaps.comp.spv
glslc --target-env=$VULKAN_TARGET voxel/voxelize.frag -o voxel/voxelize.frag.spv
glslc --target-env=$VULKAN_TARGET voxel/voxelize.geom -o voxel/voxelize.geom.spv
glslc --target-env=$VULKAN_TARGET voxel/voxelize.vert -o voxel/voxelize.vert.spv

glslc --target-env=$VULKAN_TARGET vct/voxelize.frag -o vct/voxelize.frag.spv
glslc --target-env=$VULKAN_TARGET vct/voxelize.geom -o vct/voxelize.geom.spv
glslc --target-env=$VULKAN_TARGET vct/voxelize.vert -o vct/voxelize.vert.spv
glslc --target-env=$VULKAN_TARGET vct/clear_voxels.comp -o vct/clear_voxels.comp.spv

popd
