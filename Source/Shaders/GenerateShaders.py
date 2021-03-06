# Copyright (c) 2021 Sultim Tsyrendashiev
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


import sys
import os
import subprocess
import pathlib


CACHE_FOLDER_PATH     = "Build/"
CACHE_FILE_NAME       = "GenerateShadersCache.txt"
EXTENSIONS            = [ ".comp", ".vert", "frag", ".rgen", ".rahit", ".rchit", ".rmiss" ]
DEPENDENCY_EXTENSIONS = [ ".h", ".inl" ]
DEPENDENCY_FOLDERS    = [ "../Generated/" ]
DEPENDENCY_IGNORE     = [ "BlueNoiseFileNames.h", "ShaderCommonC.h", "ShaderCommonCFramebuf.h" ]


def main():
    if "--help" in sys.argv or "-help" in sys.argv:
        print("-rebuild  : clear cache and rebuild all shaders")
        print("-gencomm  : invoke GenerateShaderCommon.py script")
        print("-r        : same as \'-rebuild\"")
        print("-g        : same as \'-gencomm\"")
        return

    forceRebuild = False
    if "-rebuild" in sys.argv or "-r" in sys.argv:
        forceRebuild = True
    if "-gencomm" in sys.argv or "-g" in sys.argv:
        subprocess.run(["python", "../Generated/GenerateShaderCommon.py", "--path", "../Generated/"])

    if not os.path.exists(CACHE_FOLDER_PATH):
        try:
            os.mkdir(CACHE_FOLDER_PATH)
        except OSError:
            print("> Coudn't create cache folder")
            return

    if not os.path.exists(CACHE_FOLDER_PATH + CACHE_FILE_NAME):
        try:
            with open(CACHE_FOLDER_PATH + CACHE_FILE_NAME, "w"): pass
        except OSError:
            print("> Coudn't create cache file")
            return
    with open(CACHE_FOLDER_PATH + CACHE_FILE_NAME, "r+") as cacheFile:
        cache = {}

        if not forceRebuild:
            try:
                for line in cacheFile:
                    # filename + st_mtime
                    nm = line.split()
                    if len(nm) >= 2:
                        cache[nm[0]] = int(nm[1])
            except:
                cache = {}
    
    wereDependentModified = False
    wasAnyShaderRebuilt = False
    errorCount = 0

    for folder in [""] + DEPENDENCY_FOLDERS:
        if not forceRebuild:
            print("> Checking dependency files in " + ("current folder" if folder == "" else folder))

        fileList = os.listdir() if folder == "" else os.listdir(folder)
        for otherFolderFilename in fileList:
            if otherFolderFilename in DEPENDENCY_IGNORE:
                continue

            filename = folder + otherFolderFilename
            isDependentOn = any([filename.endswith(ext) for ext in DEPENDENCY_EXTENSIONS])

            if not isDependentOn:
                continue

            if ' ' in folder + filename:
                print("> File \"" + folder + filename + "\" has spaces in its path. Skipping.")
                continue

            lastModifTime = pathlib.Path(filename).stat().st_mtime

            isOutdated = filename in cache and int(lastModifTime) != cache[filename]

            if filename not in cache or isOutdated:
                # if dependency files were not cached or were modified, rebuild all
                wereDependentModified = True

            cache[filename] = lastModifTime

    if wereDependentModified and not forceRebuild:
        print("> Dependency files were modified. Rebuilding all...")
    # print()

    for filename in os.listdir():
        isShader = any([filename.endswith(ext) for ext in EXTENSIONS])

        if not isShader:
            continue

        if ' ' in filename:
            print("> File \"" + filename + "\" has spaces in its name. Skipping.")
            continue

        lastModifTime = pathlib.Path(filename).stat().st_mtime
        isOutdated = filename in cache and int(lastModifTime) != cache[filename]

        if filename not in cache or isOutdated or wereDependentModified:
            print("> Building " + filename)

            r = subprocess.run([
                "glslc", "--target-env=vulkan1.2", 
                "-O", 
                "-I", "../Generated",
                filename, 
                "-o", "../../Build/" + filename + ".spv"], 
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

            if len(r.stdout) > 0:
                print(r.stdout)
                errorCount += 1
                if filename in cache:
                    del cache[filename]
            else:
                cache[filename] = lastModifTime

            wasAnyShaderRebuilt = True

    with open(CACHE_FOLDER_PATH + CACHE_FILE_NAME, "w") as cacheFile:
        for name, tm in cache.items():
            cacheFile.write(name + " " + str(int(tm)) + "\n")

    #if wereDependentModified:
    #    print()

    if errorCount > 0:
        print("> " + str(errorCount) + (" shader build failed." if errorCount == 1 else " shader builds failed."))
    elif not wasAnyShaderRebuilt:
        print("> Everything is up-to-date.")


# main
if __name__ == "__main__":
    main()