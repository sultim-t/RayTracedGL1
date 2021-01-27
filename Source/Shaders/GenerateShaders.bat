glslc --target-env=vulkan1.2 -O -I "../Generated" BasicAnyHit.rahit -o ../../Build/BasicAnyHit.rahit.spv 
glslc --target-env=vulkan1.2 -O -I "../Generated" BasicClosestHit.rchit -o ../../Build/BasicClosestHit.rchit.spv 
glslc --target-env=vulkan1.2 -O -I "../Generated" BasicMiss.rmiss -o ../../Build/BasicMiss.rmiss.spv 
glslc --target-env=vulkan1.2 -O -I "../Generated" BasicRaygen.rgen -o ../../Build/BasicRaygen.rgen.spv 
glslc --target-env=vulkan1.2 -O -I "../Generated" BasicShadowCheck.rmiss -o ../../Build/BasicShadowCheck.rmiss.spv
glslc --target-env=vulkan1.2 -O -I "../Generated" Rasterizer.vert -o ../../Build/Rasterizer.vert.spv
glslc --target-env=vulkan1.2 -O -I "../Generated" Rasterizer.frag -o ../../Build/Rasterizer.frag.spv