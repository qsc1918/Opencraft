#pragma once
#include <cstddef>
#include <cstdint>

// 最小的 OpenGL 3.3 核心函数表。项目不依赖 GLEW/GLAD，手动只加载实际用到的函数，
// 这样 MSVC（CI）和 MinGW 都能编译，玩家机器也不需要额外运行库。
namespace gfxgl {

using Enum = unsigned int;
using Uint = unsigned int;
using Int = int;
using Sizei = int;
using Float = float;
using Bitfield = unsigned int;
using Boolean = unsigned char;
using Char = char;
using Sizeiptr = ptrdiff_t;
using Intptr = ptrdiff_t;
using Handle = void*;

constexpr Enum kFragShader = 0x8B30;
constexpr Enum kVertShader = 0x8B31;
constexpr Enum kCompileStatus = 0x8B81;
constexpr Enum kLinkStatus = 0x8B82;
constexpr Enum kArrayBuffer = 0x8892;
constexpr Enum kElementBuffer = 0x8893;
constexpr Enum kStaticDraw = 0x88E4;
constexpr Enum kDynamicDraw = 0x88E8;
constexpr Enum kFloat = 0x1406;
constexpr Enum kShort = 0x1402;
constexpr Enum kInt = 0x1404;
constexpr Enum kUnsignedInt = 0x1405;
constexpr Enum kUnsignedByte = 0x1401;
constexpr Enum kRgba = 0x1908;
constexpr Enum kBgra = 0x80E1;
constexpr Enum kTexture2D = 0x0DE1;
constexpr Enum kTexture0 = 0x84C0;
constexpr Enum kTexMinFilter = 0x2801;
constexpr Enum kTexMagFilter = 0x2800;
constexpr Enum kTexWrapS = 0x2802;
constexpr Enum kTexWrapT = 0x2803;
constexpr Enum kLinear = 0x2601;
constexpr Enum kNearest = 0x2600;
constexpr Enum kLinearMipmapLinear = 0x2703;
constexpr Enum kClampToEdge = 0x812F;
constexpr Enum kColorBufferBit = 0x00004000;
constexpr Enum kDepthBufferBit = 0x00000100;
constexpr Enum kDepthTest = 0x0B71;
constexpr Enum kBlend = 0x0BE2;
constexpr Enum kCullFace = 0x0B44;
constexpr Enum kSrcAlpha = 0x0302;
constexpr Enum kOneMinusSrcAlpha = 0x0303;
constexpr Enum kOne = 1;
constexpr Enum kZero = 0;
constexpr Enum kBack = 0x0405;
constexpr Enum kFront = 0x0404;
constexpr Enum kCcw = 0x0901;
constexpr Enum kCw = 0x0900;
constexpr Enum kTriangles = 0x0004;
constexpr Enum kVersion = 0x1F02;
constexpr Enum kRenderer = 0x1F01;
constexpr Enum kVendor = 0x1F00;
constexpr Enum kShadingLanguageVersion = 0x8B8C;
constexpr Enum kSrgb8Alpha8 = 0x8C43;
constexpr Enum kRgba8 = 0x8058;
constexpr Enum kFramebufferSrgb = 0x8DB9;
constexpr Enum kMaxTextureSize = 0x0D33;
constexpr Enum kPixelUnpackAlignment = 0x0CF5;
constexpr Enum kPackAlignment = 0x0D05;
constexpr Enum kLessEqual = 0x0203;
constexpr Enum kTextureMaxLevel = 0x813D;
constexpr Enum kFalse = 0;
constexpr Enum kTrue = 1;

using Proc = void (*)();

// 函数指针类型：加载层用它们声明全局函数指针，避免依赖系统 glext.h。
using GetIntegervFn = void (*)(Enum, Int*);
using GetStringFn = const unsigned char* (*)(Enum);
using ViewportFn = void (*)(Int, Int, Sizei, Sizei);
using ScissorFn = void (*)(Int, Int, Sizei, Sizei);
using ClearColorFn = void (*)(Float, Float, Float, Float);
using ClearDepthFn = void (*)(double);
using ClearFn = void (*)(Bitfield);
using EnableFn = void (*)(Enum);
using DisableFn = void (*)(Enum);
using DepthFuncFn = void (*)(Enum);
using DepthMaskFn = void (*)(Boolean);
using BlendFuncFn = void (*)(Enum, Enum);
using CullFaceFn = void (*)(Enum);
using FrontFaceFn = void (*)(Enum);
using PixelStoreiFn = void (*)(Enum, Int);
using FlushFn = void (*)();
using FinishFn = void (*)();
using GetErrorFn = unsigned int (*)();
using GenBuffersFn = void (*)(Sizei, Uint*);
using DeleteBuffersFn = void (*)(Sizei, const Uint*);
using BindBufferFn = void (*)(Enum, Uint);
using BufferDataFn = void (*)(Enum, Sizeiptr, const void*, Enum);
using BufferSubDataFn = void (*)(Enum, Intptr, Sizeiptr, const void*);
using GenVertexArraysFn = void (*)(Sizei, Uint*);
using DeleteVertexArraysFn = void (*)(Sizei, const Uint*);
using BindVertexArrayFn = void (*)(Uint);
using GenTexturesFn = void (*)(Sizei, Uint*);
using DeleteTexturesFn = void (*)(Sizei, const Uint*);
using BindTextureFn = void (*)(Enum, Uint);
using ActiveTextureFn = void (*)(Enum);
using TexImage2DFn = void (*)(Enum, Int, Int, Sizei, Sizei, Int, Enum, Enum, const void*);
using TexSubImage2DFn = void (*)(Enum, Int, Int, Int, Sizei, Sizei, Enum, Enum, const void*);
using GenerateMipmapFn = void (*)(Enum);
using TexParameteriFn = void (*)(Enum, Enum, Int);
using TexParameterfFn = void (*)(Enum, Enum, Float);
using CreateShaderFn = Uint (*)(Enum);
using DeleteShaderFn = void (*)(Uint);
using ShaderSourceFn = void (*)(Uint, Sizei, const Char* const*, const Int*);
using CompileShaderFn = void (*)(Uint);
using GetShaderivFn = void (*)(Uint, Enum, Int*);
using GetShaderInfoLogFn = void (*)(Uint, Sizei, Sizei*, Char*);
using CreateProgramFn = Uint (*)();
using DeleteProgramFn = void (*)(Uint);
using AttachShaderFn = void (*)(Uint, Uint);
using LinkProgramFn = void (*)(Uint);
using GetProgramivFn = void (*)(Uint, Enum, Int*);
using GetProgramInfoLogFn = void (*)(Uint, Sizei, Sizei*, Char*);
using UseProgramFn = void (*)(Uint);
using BindAttribLocationFn = void (*)(Uint, Uint, const Char*);
using GetUniformLocationFn = Int (*)(Uint, const Char*);
using Uniform1iFn = void (*)(Int, Int);
using Uniform4fFn = void (*)(Int, Float, Float, Float, Float);
using Uniform1fvFn = void (*)(Int, Sizei, const Float*);
using Uniform2fvFn = void (*)(Int, Sizei, const Float*);
using Uniform4fvFn = void (*)(Int, Sizei, const Float*);
using UniformMatrix4fvFn = void (*)(Int, Sizei, Boolean, const Float*);
using VertexAttribPointerFn = void (*)(Uint, Int, Enum, Boolean, Sizei, const void*);
using VertexAttribIPointerFn = void (*)(Uint, Int, Enum, Sizei, const void*);
using EnableVertexAttribArrayFn = void (*)(Uint);
using DisableVertexAttribArrayFn = void (*)(Uint);
using DrawArraysFn = void (*)(Enum, Int, Sizei);
using DrawElementsFn = void (*)(Enum, Sizei, Enum, const void*);
using ReadPixelsFn = void (*)(Int, Int, Sizei, Sizei, Enum, Enum, void*);

extern Proc (*getProc)(const char* name);

// 每帧都会用到，放成全局变量避免每次调用去查表。
extern void (*GetIntegerv)(Enum, Int*);
extern const unsigned char* (*GetString)(Enum);
extern void (*Viewport)(Int, Int, Sizei, Sizei);
extern void (*Scissor)(Int, Int, Sizei, Sizei);
extern void (*ClearColor)(Float, Float, Float, Float);
extern void (*ClearDepth)(double);
extern void (*Clear)(Bitfield);
extern void (*Enable)(Enum);
extern void (*Disable)(Enum);
extern void (*DepthFunc)(Enum);
extern void (*DepthMask)(Boolean);
extern void (*BlendFunc)(Enum, Enum);
extern void (*CullFace)(Enum);
extern void (*FrontFace)(Enum);
extern void (*PixelStorei)(Enum, Int);
extern void (*Flush)();
extern void (*Finish)();
extern unsigned int (*GetError)();

extern void (*GenBuffers)(Sizei, Uint*);
extern void (*DeleteBuffers)(Sizei, const Uint*);
extern void (*BindBuffer)(Enum, Uint);
extern void (*BufferData)(Enum, Sizeiptr, const void*, Enum);
extern void (*BufferSubData)(Enum, Intptr, Sizeiptr, const void*);

extern void (*GenVertexArrays)(Sizei, Uint*);
extern void (*DeleteVertexArrays)(Sizei, const Uint*);
extern void (*BindVertexArray)(Uint);

extern void (*GenTextures)(Sizei, Uint*);
extern void (*DeleteTextures)(Sizei, const Uint*);
extern void (*BindTexture)(Enum, Uint);
extern void (*ActiveTexture)(Enum);
extern void (*TexImage2D)(Enum, Int, Int, Sizei, Sizei, Int, Enum, Enum, const void*);
extern void (*TexSubImage2D)(Enum, Int, Int, Int, Sizei, Sizei, Enum, Enum, const void*);
extern void (*GenerateMipmap)(Enum);
extern void (*TexParameteri)(Enum, Enum, Int);
extern void (*TexParameterf)(Enum, Enum, Float);

extern Uint (*CreateShader)(Enum);
extern void (*DeleteShader)(Uint);
extern void (*ShaderSource)(Uint, Sizei, const Char* const*, const Int*);
extern void (*CompileShader)(Uint);
extern void (*GetShaderiv)(Uint, Enum, Int*);
extern void (*GetShaderInfoLog)(Uint, Sizei, Sizei*, Char*);
extern Uint (*CreateProgram)();
extern void (*DeleteProgram)(Uint);
extern void (*AttachShader)(Uint, Uint);
extern void (*LinkProgram)(Uint);
extern void (*GetProgramiv)(Uint, Enum, Int*);
extern void (*GetProgramInfoLog)(Uint, Sizei, Sizei*, Char*);
extern void (*UseProgram)(Uint);
extern void (*BindAttribLocation)(Uint, Uint, const Char*);
extern Int (*GetUniformLocation)(Uint, const Char*);
extern void (*Uniform1i)(Int, Int);
extern void (*Uniform4f)(Int, Float, Float, Float, Float);
extern void (*Uniform1fv)(Int, Sizei, const Float*);
extern void (*Uniform2fv)(Int, Sizei, const Float*);
extern void (*Uniform4fv)(Int, Sizei, const Float*);
extern void (*UniformMatrix4fv)(Int, Sizei, Boolean, const Float*);

extern void (*VertexAttribPointer)(Uint, Int, Enum, Boolean, Sizei, const void*);
extern void (*VertexAttribIPointer)(Uint, Int, Enum, Sizei, const void*);
extern void (*EnableVertexAttribArray)(Uint);
extern void (*DisableVertexAttribArray)(Uint);
extern void (*DrawArrays)(Enum, Int, Sizei);
extern void (*DrawElements)(Enum, Sizei, Enum, const void*);

extern void (*ReadPixels)(Int, Int, Sizei, Sizei, Enum, Enum, void*);

// 由平台层提供取址函数（wglGetProcAddress / glXGetProcAddress）。
// legacy=true 时允许 VAO / 整数属性等 3.3 函数缺失，老设备档不会调用它们。
bool LoadFunctions(Proc (*getProcAddress)(const char* name), bool legacy = false);
void ClearFunctions();

} // namespace gfxgl
