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


CACHE_FOLDER_PATH           = "Build/"
CACHE_FILE_NAME             = "GenerateShadersCache.txt"
EXTENSIONS                  = [ ".comp", ".vert", "frag", ".rgen", ".rahit", ".rchit", ".rmiss" ]
DEPENDENCY_EXTENSIONS       = [ ".h", ".inl" ]
DEPENDENCY_FOLDERS          = { "", "../Generated/" }
DEPENDENCY_FOLDERS_IGNORE   = [ CACHE_FOLDER_PATH, ".vscode/" ]
DEPENDENCY_IGNORE           = [ "BlueNoiseFileNames.h", "ShaderCommonC.h", "ShaderCommonCFramebuf.h" ]


CACHE_FILE_DEPENDENCY_MAP_SEPARATOR_LINE = "DEPENDENCY\n"


MARKED_FILES = []
def wereDependentModified(dependencyMap, modifiedDependent, cache, baseFile, firstTime=True):
    global MARKED_FILES
    if firstTime:
        MARKED_FILES = []

    for dpd in dependencyMap[baseFile]:
        if dpd in modifiedDependent or dpd not in cache:
            return True
        elif dpd not in MARKED_FILES:
            MARKED_FILES.append(dpd)
            if wereDependentModified(dependencyMap, modifiedDependent, cache, dpd, firstTime=False):
                return True

    return False


def printInPowerShell(msg, color):
    p = subprocess.run([
        "PowerShell",
        "Write-Host",
        "\"" + msg + "\"",
        "-ForegroundColor", color
    ])


def getDependentFoldersProcArg():
    return [a for p in DEPENDENCY_FOLDERS if p != "" for a in ("-I", p)]


def abspath(filename):
    return os.path.abspath(filename).replace('\\','/')


def getAllSubfolders(folder):
    folder = folder if folder != "" else "."
    return {os.path.relpath(root).replace('\\','/') + '/' for root, _, _ in os.walk(folder) if abspath(root) != abspath(folder)}


def fillDependencyFolders():
    global DEPENDENCY_FOLDERS

    fs = DEPENDENCY_FOLDERS
    for f in DEPENDENCY_FOLDERS:
        fs = fs.union(getAllSubfolders(f))

    for i in DEPENDENCY_FOLDERS_IGNORE:
        fs.discard(i)

    DEPENDENCY_FOLDERS = fs


def main():
    if "--help" in sys.argv or "-help" in sys.argv or "-h" in sys.argv or "--h" in sys.argv:
        print("-rebuild  : clear cache and rebuild all shaders")
        print("-gencomm  : invoke GenerateShaderCommon.py script")
        print("-psout    : use PowerShell for printing colored output")
        print("-r        : same as \"-rebuild\"")
        print("-g        : same as \"-gencomm\"")
        print("-ps       : same as \"-psout\"")
        return

    forceRebuild = False
    powerShellOutput = False
    if "-rebuild" in sys.argv or "--rebuild" in sys.argv or "-r" in sys.argv or "--r" in sys.argv:
        forceRebuild = True
    if "-gencomm" in sys.argv or "--gencomm" in sys.argv or "-g" in sys.argv or "--g" in sys.argv:
        subprocess.run(["python", "../Generated/GenerateShaderCommon.py", "--path", "../Generated/"])
    if "-psout" in sys.argv or "--psout" in sys.argv or "-ps" in sys.argv or "--ps" in sys.argv:
        powerShellOutput = True
    #elif len(sys.argv) > 1:
    #    print("> Couldn't parse arguments")
    #    return

    fillDependencyFolders()

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
        dependencyMap = {}

        if not forceRebuild:
            try:
                parsingDpdncy = False
                for line in cacheFile:
                    if line == CACHE_FILE_DEPENDENCY_MAP_SEPARATOR_LINE:
                        parsingDpdncy = True
                    else:
                        words = line.split()
                        if not parsingDpdncy and len(words) >= 2:
                            # filename + st_mtime
                            cache[words[0]] = int(words[1])

                        if parsingDpdncy:
                            if len(words) >= 2:
                                # filename + (list of files it dependent on)
                                checkedDpds = set()
                                dpds = set(words[1:])
                                for dpd in dpds:
                                    if os.path.exists(dpd):
                                        checkedDpds.add(dpd)
                                dependencyMap[words[0]] = checkedDpds
                            else:
                                dependencyMap[words[0]] = set()
            except:
                cache = {}
                dependencyMap = {}
    
    modifiedDependent = set()

    msgWasAnyShaderRebuilt = False
    msgErrorCount = 0

    for folder in DEPENDENCY_FOLDERS:
        if not forceRebuild:
            print("> Checking dependency files in " + ("current folder" if folder == "" else folder))

        fileList = os.listdir() if folder == "" else os.listdir(folder)
        for otherFolderFilename in fileList:
            if otherFolderFilename in DEPENDENCY_IGNORE:
                continue

            filename = abspath(folder + otherFolderFilename)
            isDependentOn = any([filename.endswith(ext) for ext in DEPENDENCY_EXTENSIONS])

            if not isDependentOn:
                continue

            if ' ' in folder + filename:
                print("> File \"" + folder + filename + "\" has spaces in its path. Skipping.")
                continue

            lastModifTime = int(pathlib.Path(filename).stat().st_mtime)

            isOutdated = filename in cache and lastModifTime != cache[filename]

            if filename not in cache or isOutdated:
                modifiedDependent.add(filename)

            cache[filename] = lastModifTime

            if filename not in dependencyMap or isOutdated:
                dependencyMap[filename] = set()

                with open(filename, "r") as dpd:
                    for line in dpd:
                        if "#include" in line:
                            dpdFile = line.split("\"")[1]
                            for dpdFolder in DEPENDENCY_FOLDERS:
                                dpd = abspath(dpdFolder + dpdFile)
                                if os.path.exists(dpd) and dpd != filename:
                                    dependencyMap[filename].add(dpd)

    #if wereDependentModified and not forceRebuild:
    #    print("> Dependency files were modified. Rebuilding all...")
    # print()

    for filenameRelative in os.listdir():
        filename = abspath(filenameRelative)
        isShader = any([filename.endswith(ext) for ext in EXTENSIONS])

        if not isShader:
            continue

        if ' ' in filename:
            print("> File \"" + filename + "\" has spaces in its name. Skipping.")
            continue

        lastModifTime = int(pathlib.Path(filename).stat().st_mtime)
        isOutdated = filename in cache and lastModifTime != cache[filename]

        if filename not in dependencyMap or isOutdated:
            dependencyMap[filename] = set()

            with open(filename, "r") as dpd:
                for line in dpd:
                    if "#include" in line:
                        dpdFile = line.split("\"")[1]
                        for dpdFolder in DEPENDENCY_FOLDERS:
                            dpd = abspath(dpdFolder + dpdFile)
                            if os.path.exists(dpd):
                                dependencyMap[filename].add(dpd)

        if filename not in cache or isOutdated or wereDependentModified(dependencyMap, modifiedDependent, cache, filename):
            print("> Building " + os.path.basename(filename))

            r = subprocess.run([
                "glslc", "--target-env=vulkan1.2"
                ] + getDependentFoldersProcArg() + [
                filename, 
                "-o", "../../Build/" + os.path.basename(filename) + ".spv"], 
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

            if len(r.stdout) > 0:
                if powerShellOutput:
                    printInPowerShell(r.stdout, "Red")
                else:
                    print(r.stdout)

                msgErrorCount += 1
                if filename in cache:
                    del cache[filename]
            else:
                cache[filename] = lastModifTime

            msgWasAnyShaderRebuilt = True

    with open(CACHE_FOLDER_PATH + CACHE_FILE_NAME, "w") as cacheFile:
        for name, tm in cache.items():
            cacheFile.write(name + " " + str(tm) + "\n")
        cacheFile.write(CACHE_FILE_DEPENDENCY_MAP_SEPARATOR_LINE)
        for name, arr in dependencyMap.items():
            arrStr = " ".join(arr)
            cacheFile.write(name + " " + arrStr + "\n")

    #if wereDependentModified:
    #    print()

    msg = ""
    color = ""

    if msgErrorCount > 0:
        msg = "> " + str(msgErrorCount) + (" shader build failed." if msgErrorCount == 1 else " shader builds failed.")
        color = "DarkRed"
    elif not msgWasAnyShaderRebuilt:
        msg = "> Everything is up-to-date."
        color = "Green"
    else:
        msg = "> Done."
        color = "Green"

    if powerShellOutput:
        printInPowerShell(msg, color)
    else:
        print(msg)


# main
if __name__ == "__main__":
    main()