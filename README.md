# RTGL1

RTGL1 is a library that aims to simplify the process of porting 3D applications to *real-time path tracing*, via hardware accelerated ray tracing, denoising algorithms (A-SVGF) and advanced sampling algorithms (ReSTIR, ReSTIR GI) to improve the image quality by aggressively reusing spatio-temporal data.

## RUG1 (development)

RUG1 (stands for **R**ay-traced **U**pdated **G**raphics Library **1**) is the next generation of RTGL1. 

Goals:
* overriding not just textures, but 3D models with animations
* thinner interface
* internal restructuring

## Build
1. Requirements:
    * 64-bit CPU
    * GPU with a ray tracing support
    * [Git](https://github.com/git-for-windows/git/releases)
    * [CMake](https://cmake.org/download/)
    * [Vulkan SDK](https://vulkan.lunarg.com/)
    * [Python 3](https://www.python.org/downloads/) (for building the shaders)
 
1. Clone the repository
    * `git clone https://github.com/sultim-t/RayTracedGL1.git`

1. Configure with CMake
    * on Windows, with Visual Studio: 
        * open the folder as CMake project
    * otherwise:
        * specify windowing systems to build the library with, by enabling some of the CMake options:
            * `RG_WITH_SURFACE_WIN32`
            * `RG_WITH_SURFACE_METAL`
            * `RG_WITH_SURFACE_WAYLAND`
            * `RG_WITH_SURFACE_XCB`
            * `RG_WITH_SURFACE_XLIB`  
        * configure
        ```
        mkdir Build
        cd Build
        cmake ..
        ```
        * but make sure that projects that use RTGL1 can find the compiled dynamic library, as it usually assumed that it's in `Build/x64-Debug` or `Build/x64-Release`

1. Build
    * `cmake --build .`

1. Build shaders
    * Run `Source/Shaders/GenerateShaders.py` with Python3, it will generate SPIR-V files to `Build` folder

### Notes:
* RTGL1 requires a set of blue noise images on start-up: `RgInstanceCreateInfo::pBlueNoiseFilePath`. A ready-to-use resource can be found here: `Tools/BlueNoise_LDR_RGBA_128.ktx2`


## Tools

### Shader development

RTGL1 supports shader hot-reloading (a target application sets `RgStartFrameInfo::requestShaderReload=true` in runtime).

But to ease the process of *building* the shaders, instead of running `GenerateShaders.py` from a terminal manually, you can install [Visual Studio Code](https://code.visualstudio.com/) and [Script Runner extension](https://marketplace.visualstudio.com/items?itemName=easterapps.script-runner) to it. Open `Sources/Shaders` folder, add such config to VS Code's `.json` settings file (TODO: VS Code workspace).
```
"script-runner.definitions": { "commands": [ { "identifier": "shaderBuild", "description": "Build shaders", "command": "cls; python .\\GenerateShaders.py -ps", "working_directory": "${workspaceFolder}", }, { "identifier": "shaderGenAndBuild", "description": "Build shaders with generating common files", "command": "cls; python .\\GenerateShaders.py -ps -g", "working_directory": "${workspaceFolder}", } ], },
```
Then assign hotkeys to `shaderBuild` and `shaderGenAndBuild` commands in `File->Preferences->Keyboard Shortcuts`.

### Textures
Some games don't have PBR materials, but to add them, RTGL1 provides 'texture overriding' functionality: application requests to upload an original texture and specifies its name, then RTGL1 tries to find files with such name (appending some suffixes, e.g. `_n` for normal maps, or none for albedo maps) and loads them instead of original ones. These files are in `.ktx2` format with a specific compression and contain image data. 

To generate such textures: 
1. [Compressonator CLI](https://gpuopen.com/compressonator/) and `Python3` are required
1. Create a folder, put `Tools/CreateKTX2.py`, create folder named `Raw` and `Compressed`.
1. The script:
   1. scans files (with `INPUT_EXTENSIONS`) in `Raw` folder
   1. generates corresponding `.ktx2` file to `Compressed` folder, preserving the hierarchy

On RTGL1 initialization, `RgInstanceCreateInfo::pOverridenTexturesFolderPath` should contain a path to the `Compressed` folder. 



# Projects
* https://github.com/sultim-t/Serious-Engine-RT/releases
* https://github.com/sultim-t/prboom-plus-rt

## Screenshots
![Screenshot 00](/Doc/Screenshots/Screenshot_00.png)
![Screenshot 01](/Doc/Screenshots/Screenshot_01.png)
