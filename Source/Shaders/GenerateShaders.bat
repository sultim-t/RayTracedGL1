for %%f in (*.vert, *frag, *.rgen, *.rahit, *.rchit, *.rmiss) do glslc --target-env=vulkan1.2 -O -I "../Generated" %%f -o ../../Build/%%~nf%%~xf.spv
