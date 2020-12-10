# This script generates two separate header files for C and GLSL but with identical data

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
    (TYPE_FLOAT, 22): "mat22",
    (TYPE_FLOAT, 23): "mat23",
    (TYPE_FLOAT, 32): "mat32",
    (TYPE_FLOAT, 33): "mat33",
    (TYPE_FLOAT, 34): "mat34",
    (TYPE_FLOAT, 43): "mat43",
    (TYPE_FLOAT, 44): "mat44",
}

TAB_STR = "    "

USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE = False




# ---
# User defined constants
# ---
CONST = {
    "MAX_STATIC_VERTEX_COUNT": 1 << 21,
    "MAX_DYNAMIC_VERTEX_COUNT": 1 << 21,
    "BINDING_VERTEX_BUFFER_STATIC": 0,
    "BINDING_VERTEX_BUFFER_DYNAMIC": 1,
    "BINDING_GLOBAL_UNIFORM": 0,
}


# ---
# Constants that must be defined before including generated header in GLSL
# ---
MUST_BE_DEFINED_GLSL = [
    "DESC_SET_VERTEX_DATA",
    "DESC_SET_GLOBAL_UNIFORM"
]


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
    (TYPE_UINT32,    1,     "materialId",       1),
]

GLOBAL_UNIFORM_STRUCT = [
    (TYPE_FLOAT,    44,     "viewProj",         1),
    (TYPE_FLOAT,    44,     "viewProjPrev",     1),
    (TYPE_UINT32,   1,      "positionsStride",  1),
    (TYPE_UINT32,   1,      "normalsStride",    1),
    (TYPE_UINT32,   1,      "texCoordsStride",  1),
    (TYPE_UINT32,   1,      "colorsStride",     1),
]

# structTypeName: (structDefinition, onlyForGLSL)
STRUCTS = {
    "VertexBufferStatic":   (STATIC_BUFFER_STRUCT,      False),
    "VertexBufferDynamic":  (DYNAMIC_BUFFER_STRUCT,     False),
    "Triangle":             (TRIANGLE_STRUCT,           True),
    "GlobalUniform":        (GLOBAL_UNIFORM_STRUCT,     False),
}

# ---
# User defined buffers: uniform, storage buffer
# ---

BUFFERS = {
    # blockName: (descSet, binding, memoryQualifiers, storageQualifier,
    #            structTypeName, structName, variableStride)
    # variableStride: If count > 1 and dimension is 2, 3 or 4
    #                 then member will be using only base types and getters will be generated.
    #                 It can be used if stride is defined by the variable:
    #                 if USE_BASE_STRUCT_NAME_IN_VARIABLE_STRIDE:
    #                     ("globalUniform." + + structName + MemberName + + "Stride")
    #                 else:
    #                     ("globalUniform." + memberName + + "Stride")
    "VertexBufferStatic_BT":     ("DESC_SET_VERTEX_DATA", "BINDING_VERTEX_BUFFER_STATIC", "readonly",
                                  "buffer", "VertexBufferStatic", "staticVertices", True),
    "VertexBufferDynamic_BT":    ("DESC_SET_VERTEX_DATA", "BINDING_VERTEX_BUFFER_DYNAMIC", "readonly",
                                  "buffer", "VertexBufferDynamic", "dynamicVertices", True),
    "GlobalUniform_BT":          ("DESC_SET_GLOBAL_UNIFORM", "BINDING_GLOBAL_UNIFORM", "readonly",
                                  "uniform", "GlobalUniform", "globalUniform", False),
}





def main():
    # with open('ShaderConfig.csv', newline='') as csvfile:
    with open("ShaderCommonC.h", "w") as f:
        writeToC(f)
    with open("ShaderCommonGLSL.h", "w") as f:
        writeToGLSL(f)


def getAllConstDefs():
    return "\n".join([
        "#define %s (%d)" % (name, value)
        for name, value in CONST.items()
    ]) + "\n\n"


def getErrorsIfndef(vars):
    return "\n".join([
        "#ifndef %s\n"
        "    #error Define \"%s\" before including this header.\n"
        "#endif" % (v, v)
        for v in vars
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
                r += "%s %s[%d][%d]" % (typeNames[baseType], mname, dim // 10, dim % 10)
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


def getLayout(blockName, definition):
    r = "layout(set = %s,\n" \
        "    binding = %s)\n" \
        "    %s %s " + blockName + "\n" \
        "{\n" \
        "    %s %s;\n" \
        "}\n"

    return r % definition[:6]


def getAllLayouts():
    return "\n".join(
        getLayout(blockName, definition)
        for blockName, definition in BUFFERS.items()
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
    ret = GLSL_TYPE_NAMES[(baseType, dim)] + "("
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
        getGetter(bufDef[5], baseType, dim, mname)
        for blockName, bufDef in BUFFERS.items()
        # for each member in struct
        for baseType, dim, mname, count in STRUCTS[bufDef[4]][0]
        # if using variableStride
        if count > 1 and dim > 1 and bufDef[6]
    ) + "\n"


# def getAllSetters():
#     return "\n".join(
#         getSetter()
#         for _, definition in BUFFERS.items()
#         if "readonly" not in definition[2]
#     ) + "\n"


def writeToC(f):
    f.write("#pragma once\n\n")
    f.write(getAllConstDefs())
    f.write(getAllStructDefs(C_TYPE_NAMES))


def writeToGLSL(f):
    f.write(getAllConstDefs())
    f.write(getErrorsIfndef(MUST_BE_DEFINED_GLSL))
    f.write(getAllStructDefs(GLSL_TYPE_NAMES))
    f.write(getAllLayouts())
    f.write(getAllGetters())

main()
