# This script generates two separate header files for C and GLSL but with identical data

import sys

TYPE_FLOAT = 0
TYPE_INT32 = 1
TYPE_UINT32 = 2

C_TYPE_NAMES = {
    TYPE_FLOAT: "float",
    TYPE_INT32: "int32_t",
    TYPE_UINT32: "uint32_t",
}

GLSL_TYPE_NAMES = {
    TYPE_FLOAT: "float",
    TYPE_INT32: "int",
    TYPE_UINT32: "uint",
    (TYPE_FLOAT, 2): "vec2",
    (TYPE_FLOAT, 3): "vec3",
    (TYPE_FLOAT, 4): "vec4",
    (TYPE_INT32, 2): "ivec2",
    (TYPE_INT32, 3): "ivec3",
    (TYPE_INT32, 4): "ivec4",
    (TYPE_UINT32, 2): "uvec2",
    (TYPE_UINT32, 3): "uvec3",
    (TYPE_UINT32, 4): "uvec4",
    (TYPE_FLOAT, 22): "mat2",
    (TYPE_FLOAT, 23): "mat2x3",
    (TYPE_FLOAT, 32): "mat3x2",
    (TYPE_FLOAT, 33): "mat3",
    (TYPE_FLOAT, 34): "mat3x4",
    (TYPE_FLOAT, 43): "mat4x3",
    (TYPE_FLOAT, 44): "mat4",
}

TAB_STR = "    "

USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE = False
USE_MULTIDIMENSIONAL_ARRAYS_IN_C = False




# ---
# User defined constants
# ---
CONST = {
    "MAX_STATIC_VERTEX_COUNT"               : 1 << 22,
    "MAX_DYNAMIC_VERTEX_COUNT"              : 1 << 21,
    "MAX_VERTEX_COLLECTOR_INDEX_COUNT"      : 1 << 22,
    "MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT" : 1 << 18,
    "MAX_VERTEX_COLLECTOR_GEOM_INFOS_COUNT" : 1 << 18,
    "MAX_TOP_LEVEL_INSTANCE_COUNT"          : 1 << 12,
    "BINDING_VERTEX_BUFFER_STATIC"          : 0,
    "BINDING_VERTEX_BUFFER_DYNAMIC"         : 1,
    "BINDING_INDEX_BUFFER_STATIC"           : 2,
    "BINDING_INDEX_BUFFER_DYNAMIC"          : 3,
    "BINDING_GEOMETRY_INSTANCES_STATIC"     : 4,
    "BINDING_GEOMETRY_INSTANCES_DYNAMIC"    : 5,
    "BINDING_GLOBAL_UNIFORM"                : 0,
    "BINDING_ACCELERATION_STRUCTURE"        : 0,
    "BINDING_STORAGE_IMAGE"                 : 0,
    "INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC"    : "1 << 0"
}



# ---
# User defined structs
# ---
# Each member is defined as a tuple (base type, dimensions, name, count).
# Dimensions:   1 - scalar, 2 - *vec2, 3 - *vec3, 4 - *vec4, [xy] - *mat[xy] (i.e. 32 - *mat32)
# If count > 1 and dimensions is 2, 3 or 4 (matrices are not supported)
# then it'll be represented as an array with size (count*dimensions).

STATIC_BUFFER_STRUCT = [
    (TYPE_FLOAT,     3,     "positions",        CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_FLOAT,     3,     "normals",          CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_FLOAT,     2,     "texCoords",        CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_UINT32,    1,     "colors",           CONST["MAX_STATIC_VERTEX_COUNT"]),
    (TYPE_UINT32,    1,     "materialIds",      CONST["MAX_STATIC_VERTEX_COUNT"] // 3),
]

DYNAMIC_BUFFER_STRUCT = [
    (TYPE_FLOAT,     3,     "positions",        CONST["MAX_DYNAMIC_VERTEX_COUNT"]),
    (TYPE_FLOAT,     3,     "normals",          CONST["MAX_DYNAMIC_VERTEX_COUNT"]),
    (TYPE_FLOAT,     2,     "texCoords",        CONST["MAX_DYNAMIC_VERTEX_COUNT"]),
    (TYPE_UINT32,    1,     "colors",           CONST["MAX_DYNAMIC_VERTEX_COUNT"]),
    (TYPE_UINT32,    1,     "materialIds",      CONST["MAX_DYNAMIC_VERTEX_COUNT"] // 3),
]

TRIANGLE_STRUCT = [
    (TYPE_FLOAT,    33,     "positions",        1),
    (TYPE_FLOAT,    33,     "normals",          1),
    (TYPE_FLOAT,    32,     "textureCoords",    1),
    (TYPE_FLOAT,     3,     "tangent",          1),
    (TYPE_UINT32,    3,     "materialIds",      1),
]

GLOBAL_UNIFORM_STRUCT = [
    (TYPE_FLOAT,    44,     "view",             1),
    (TYPE_FLOAT,    44,     "invView",          1),
    (TYPE_FLOAT,    44,     "viewPrev",         1),
    (TYPE_FLOAT,    44,     "projection",       1),
    (TYPE_FLOAT,    44,     "invProjection",    1),
    (TYPE_FLOAT,    44,     "projectionPrev",   1),
    (TYPE_UINT32,   1,      "positionsStride",  1),
    (TYPE_UINT32,   1,      "normalsStride",    1),
    (TYPE_UINT32,   1,      "texCoordsStride",  1),
    (TYPE_UINT32,   1,      "colorsStride",     1),
]

GEOM_INSTANCE_STRUCT = [
    (TYPE_FLOAT,    44,     "model",            1),
    (TYPE_UINT32,   1,      "baseVertexIndex",  1),
    (TYPE_UINT32,   1,      "baseIndexIndex",   1),
    (TYPE_UINT32,   1,      "primitiveCount",   1),
    (TYPE_UINT32,   1,      "materialId0",      1),
    (TYPE_UINT32,   1,      "materialId1",      1),
    (TYPE_UINT32,   1,      "materialId2",      1),
]

# (structTypeName): (structDefinition, onlyForGLSL)
STRUCTS = {
    "ShVertexBufferStatic":     (STATIC_BUFFER_STRUCT,      False),
    "ShVertexBufferDynamic":    (DYNAMIC_BUFFER_STRUCT,     False),
    "ShTriangle":               (TRIANGLE_STRUCT,           True),
    "ShGlobalUniform":          (GLOBAL_UNIFORM_STRUCT,     False),
    "ShGeometryInstance":       (GEOM_INSTANCE_STRUCT,      False),
}

# ---
# User defined buffers: uniform, storage buffer
# ---

GETTERS = {
    # (struct type): (member to access with)
    "ShVertexBufferStatic": "staticVertices",
    "ShVertexBufferDynamic": "dynamicVertices",
}

# ---
# User defined structs END
# ---



def main():
    generateGetSet = False
    if len(sys.argv) > 0:
        if "--getset" in sys.argv:
            generateGetSet = True
        if "--help" in sys.argv:
            print("--getset   : generate getters and setters for non-trivial members")
            return

    # with open('ShaderConfig.csv', newline='') as csvfile:
    with open("ShaderCommonC.h", "w") as f:
        writeToC(f)
    with open("ShaderCommonGLSL.h", "w") as f:
        writeToGLSL(f, generateGetSet)


def getAllConstDefs():
    return "\n".join([
        "#define %s (%s)" % (name, str(value))
        for name, value in CONST.items()
    ]) + "\n\n"


def align4(a):
    return ((a + 3) >> 2) << 2


# useVecMatTypes:
def getStruct(name, definition, typeNames):
    r = "struct " + name + "\n{\n"

    for baseType, dim, mname, count in definition:
        assert(count > 0)
        r += TAB_STR

        if count == 1:
            if dim == 1:
                r +="%s %s" % (typeNames[baseType], mname)
            elif (baseType, dim) in typeNames:
                r += "%s %s" % (typeNames[(baseType, dim)], mname)
            elif dim <= 4:
                r += "%s %s[%d]" % (typeNames[baseType], mname, dim)
            else:
                if USE_MULTIDIMENSIONAL_ARRAYS_IN_C:
                    r += "%s %s[%d][%d]" % (typeNames[baseType], mname, dim // 10, dim % 10)
                else:
                    r += "%s %s[%d]" % (typeNames[baseType], mname, (dim // 10) * (dim % 10))
        else:
            if dim > 4:
                raise Exception("If count > 1, dimensions must be in [1..4]")
            r += "%s %s[%d]" % (typeNames[baseType], mname, align4(count * dim))

        r += ";\n"

    r += "};\n"
    return r


def getAllStructDefs(typeNames):
    return "\n".join(
        getStruct(name, structDef, typeNames)
        for name, (structDef, onlyForGLSL) in STRUCTS.items()
        if not (onlyForGLSL and (typeNames == C_TYPE_NAMES))
    ) + "\n"


def capitalizeFirstLetter(s):
    return s[:1].upper() + s[1:]


# Get getter for a member with variable stride.
# Currently, stride is defined as a member in GlobalUniform
def getGetter(baseMember, baseType, dim, memberName):
    assert(2 <= dim <= 4)
    if USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE:
        strideVar = "globalUniform." + baseMember + capitalizeFirstLetter(memberName) + "Stride"
    else:
        strideVar = "globalUniform." + memberName + "Stride"

    ret = GLSL_TYPE_NAMES[(baseType, dim)] + "(\n        "
    for i in range(dim):
        ret += "%s.%s[index * %s + %d]" % (baseMember, memberName, strideVar, i)
        if i != dim - 1:
            ret += ",\n        "
    ret += ");"

    res = "%s get%s%s(uint index)\n{\n" \
        "    return " + ret + "\n}\n"

    return res % (
        GLSL_TYPE_NAMES[(baseType, dim)],
        capitalizeFirstLetter(baseMember), capitalizeFirstLetter(memberName),
    )


def getAllGetters():
    return "\n".join(
        getGetter(baseMember, baseType, dim, mname)
        for structType, baseMember in GETTERS.items()
        # for each member in struct
        for baseType, dim, mname, count in STRUCTS[structType][0]
        # if using variableStride
        if count > 1 and dim > 1
    ) + "\n"


def getSetter(baseMember, baseType, dim, memberName):
    assert(2 <= dim <= 4)
    if USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE:
        strideVar = "globalUniform." + baseMember + capitalizeFirstLetter(memberName) + "Stride"
    else:
        strideVar = "globalUniform." + memberName + "Stride"

    st = ""
    for i in range(dim):
        st += "    %s.%s[index * %s + %d] = value[%d];\n" % (baseMember, memberName, strideVar, i, i)

    res = "void set%s%s(uint index, %s value)\n{\n%s}\n"

    return res % (
        capitalizeFirstLetter(baseMember), capitalizeFirstLetter(memberName),
        GLSL_TYPE_NAMES[(baseType, dim)],
        st
    )


def getAllSetters():
    return "\n".join(
        getSetter(baseMember, baseType, dim, mname)
        for structType, baseMember in GETTERS.items()
        # for each member in struct
        for baseType, dim, mname, count in STRUCTS[structType][0]
        # if using variableStride
        if count > 1 and dim > 1
    ) + "\n"


def writeToC(f):
    f.write("#pragma once\n")
    f.write("#include <stdint.h>\n\n")
    f.write(getAllConstDefs())
    f.write(getAllStructDefs(C_TYPE_NAMES))


def writeToGLSL(f, generateGetSet):
    f.write(getAllConstDefs())
    f.write(getAllStructDefs(GLSL_TYPE_NAMES))
    if generateGetSet:
        f.write(getAllGetters())
        f.write(getAllSetters())



# main
if __name__ == "__main__":
    main()
