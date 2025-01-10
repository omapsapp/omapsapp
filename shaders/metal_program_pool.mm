#include "shaders/metal_program_pool.hpp"
#include "shaders/program_params.hpp"

#include "drape/metal/metal_gpu_program.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace gpu
{
namespace metal
{
namespace
{
struct ProgramInfo
{
  using Layout = std::map<uint8_t, uint8_t>;
  std::string const m_vertexShaderName;
  std::string const m_fragmentShaderName;
  Layout m_layout;
  
  // Layout is in the format { buffer0, buffer1, ..., bufferN }.
  // bufferX is a pair { start attribute index, end attribute index }.
  ProgramInfo(std::string && vertexShaderName, std::string && fragmentShaderName,
              std::vector<std::pair<uint8_t, uint8_t>> const & layout)
    : m_vertexShaderName(std::move(vertexShaderName))
    , m_fragmentShaderName(std::move(fragmentShaderName))
  {
    for (size_t i = 0; i < layout.size(); i++)
    {
      for (size_t j = layout[i].first; j <= layout[i].second; j++)
      {
        CHECK(m_layout.find(j) == m_layout.end(), ("Duplicate index in the layout."));
        m_layout[j] = i;
      }
    }
  }
};
  
std::array<ProgramInfo, static_cast<size_t>(SystemProgram::SystemProgramsCount)> const kMetalSystemProgramsInfo = {{
  ProgramInfo("vsCleaner", "fsClearColor", {{0, 0}}),  // ClearColor
  ProgramInfo("vsCleaner", "fsClearDepth", {{0, 0}}),  // ClearDepth
  ProgramInfo("vsCleaner", "fsClearColorAndDepth", {{0, 0}}),  // ClearColorAndDepth
}};

std::array<ProgramInfo, static_cast<size_t>(Program::ProgramsCount)> const kMetalProgramsInfo = {{
  ProgramInfo("vsColoredSymbol", "fsColoredSymbol", {{0, 2}}),                     // ColoredSymbol
  ProgramInfo("vsTexturing", "fsTexturing", {{0, 2}}),                             // Texturing
  ProgramInfo("vsMaskedTexturing", "fsMaskedTexturing", {{0, 3}}),                 // MaskedTexturing
  ProgramInfo("vsUserMark", "fsUserMark", {{0, 3}}),                               // Bookmark
  ProgramInfo("vsUserMark", "fsUserMark", {{0, 3}}),                               // BookmarkAnim
  ProgramInfo("vsTextOutlined", "fsText", {{0, 2}, {3, 4}}),                       // TextOutlined
  ProgramInfo("vsText", "fsText", {{0, 1}, {2, 3}}),                               // Text
  ProgramInfo("vsTextStaticOutlinedGui", "fsTextOutlinedGui", {{0, 4}}),           // TextStaticOutlinedGui
  ProgramInfo("vsTextOutlinedGui", "fsTextOutlinedGui", {{0, 2}, {3, 4}}),         // TextOutlinedGui
  ProgramInfo("vsArea", "fsArea", {{0, 1}}),                                       // Area
  ProgramInfo("vsArea", "fsArea", {{0, 1}}),                                       // AreaOutline
  ProgramInfo("vsArea3d", "fsArea3d", {{0, 2}}),                                   // Area3d
  ProgramInfo("vsArea3dOutline", "fsArea", {{0, 1}}),                              // Area3dOutline
  ProgramInfo("vsLine", "fsLine", {{0, 2}}),                                       // Line
  ProgramInfo("vsTransitCircle", "fsTransitCircle", {{0, 2}}),                     // TransitCircle
  ProgramInfo("vsDashedLine", "fsDashedLine", {{0, 3}}),                           // DashedLine
  ProgramInfo("vsPathSymbol", "fsPathSymbol", {{0, 2}}),                           // PathSymbol
  ProgramInfo("vsArea", "fsArea", {{0, 1}}),                                       // TransparentArea
  ProgramInfo("vsCapJoin", "fsCapJoin", {{0, 2}}),                                 // CapJoin
  ProgramInfo("vsHatchingArea", "fsHatchingArea", {{0, 2}}),                       // HatchingArea
  ProgramInfo("vsTexturingGui", "fsTexturingGui", {{0, 1}}),                       // TexturingGui
  ProgramInfo("vsRuler", "fsRuler", {{0, 2}}),                                     // Ruler
  ProgramInfo("vsAccuracy", "fsAccuracy", {{0, 1}}),                               // Accuracy
  ProgramInfo("vsMyPosition", "fsMyPosition", {{0, 1}}),                           // MyPosition
  ProgramInfo("vsSelectionLine", "fsSelectionLine", {{0, 3}}),                     // SelectionLine
  ProgramInfo("vsTransit", "fsTransit", {{0, 2}}),                                 // Transit
  ProgramInfo("vsTransitMarker", "fsTransitMarker", {{0, 2}}),                     // TransitMarker
  ProgramInfo("vsRoute", "fsRoute", {{0, 3}}),                                     // Route
  ProgramInfo("vsRoute", "fsRouteDash", {{0, 3}}),                                 // RouteDash
  ProgramInfo("vsRouteArrow", "fsRouteArrow", {{0, 2}}),                           // RouteArrow
  ProgramInfo("vsRouteMarker", "fsRouteMarker", {{0, 2}}),                         // RouteMarker
  ProgramInfo("vsCirclePoint", "fsCirclePoint", {{0, 0}, {1, 2}}),                 // CirclePoint
  ProgramInfo("vsUserMark", "fsUserMark", {{0, 3}}),                               // BookmarkAboveText
  ProgramInfo("vsUserMark", "fsUserMark", {{0, 3}}),                               // BookmarkAnimAboveText
  ProgramInfo("vsDebugRect", "fsDebugRect", {{0, 0}}),                             // DebugRect
  ProgramInfo("vsScreenQuad", "fsScreenQuad", {{0, 1}}),                           // ScreenQuad
  ProgramInfo("vsArrow3d", "fsArrow3d", {{0, 0}, {1, 1}}),                         // Arrow3d
  ProgramInfo("vsArrow3dTextured", "fsArrow3dTextured", {{0, 0}, {1, 1}, {2, 2}}), // Arrow3dTextured
  ProgramInfo("vsArrow3dShadow", "fsArrow3dShadow", {{0, 0}, {1, 1}}),             // Arrow3dShadow
  ProgramInfo("vsArrow3dShadow", "fsArrow3dOutline", {{0, 0}, {1, 1}}),            // Arrow3dOutline
  ProgramInfo("vsColoredSymbolBillboard", "fsColoredSymbol", {{0, 2}}),            // ColoredSymbolBillboard
  ProgramInfo("vsTexturingBillboard", "fsTexturing", {{0, 2}}),                    // TexturingBillboard
  ProgramInfo("vsMaskedTexturingBillboard", "fsMaskedTexturing", {{0, 3}}),        // MaskedTexturingBillboard
  ProgramInfo("vsUserMarkBillboard", "fsUserMark", {{0, 3}}),                      // BookmarkBillboard
  ProgramInfo("vsUserMarkBillboard", "fsUserMark", {{0, 3}}),                      // BookmarkAnimBillboard
  ProgramInfo("vsUserMarkBillboard", "fsUserMark", {{0, 3}}),                      // BookmarkAboveTextBillboard
  ProgramInfo("vsUserMarkBillboard", "fsUserMark", {{0, 3}}),                      // BookmarkAnimAboveTextBillboard
  ProgramInfo("vsTextOutlinedBillboard", "fsText", {{0, 2}, {3, 4}}),              // TextOutlinedBillboard
  ProgramInfo("vsTextBillboard", "fsText", {{0, 1}, {2, 3}}),                      // TextBillboard
  ProgramInfo("vsTraffic", "fsTraffic", {{0, 2}}),                                 // Traffic
  ProgramInfo("vsTrafficLine", "fsTrafficLine", {{0, 1}}),                         // TrafficLine
  ProgramInfo("vsTrafficCircle", "fsTrafficCircle", {{0, 2}}),                     // TrafficCircle
  ProgramInfo("vsSmaaEdges", "fsSmaaEdges", {{0, 1}}),                             // SmaaEdges
  ProgramInfo("vsSmaaBlendingWeight", "fsSmaaBlendingWeight", {{0, 1}}),           // SmaaBlendingWeight
  ProgramInfo("vsSmaaFinal", "fsSmaaFinal", {{0, 1}}),                             // SmaaFinal
  ProgramInfo("vsImGui", "fsImGui", {{0, 2}}),                                     // ImGui
}};

MTLVertexFormat GetFormatByDataType(MTLDataType dataType)
{
  switch (dataType)
  {
  case MTLDataTypeFloat: return MTLVertexFormatFloat;
  case MTLDataTypeFloat2: return MTLVertexFormatFloat2;
  case MTLDataTypeFloat3: return MTLVertexFormatFloat3;
  case MTLDataTypeFloat4: return MTLVertexFormatFloat4;
  default: CHECK(false, ("Unsupported vertex format."));
  }
  return MTLVertexFormatInvalid;
}
  
uint32_t GetSizeByDataType(MTLDataType dataType)
{
  switch (dataType)
  {
  case MTLDataTypeFloat: return sizeof(float);
  case MTLDataTypeFloat2: return 2 * sizeof(float);
  case MTLDataTypeFloat3: return 3 * sizeof(float);
  case MTLDataTypeFloat4: return 4 * sizeof(float);
  default: CHECK(false, ("Unsupported vertex format."));
  }
  return 0;
}
  
void GetBindings(NSArray<MTLArgument *> * arguments, int8_t & uniformsBindingIndex,
                 dp::metal::MetalGpuProgram::TexturesBindingInfo & textureBindingInfo)
{
  // Uniforms buffer must have the name "uniforms".
  NSString * kUniformsName = @"uniforms";
  // Sampler name must be constructed as concatenation of texture name and kSamplerSuffix.
  static std::string const kSamplerSuffix = "Sampler";
  
  uniformsBindingIndex = dp::metal::MetalGpuProgram::kInvalidBindingIndex;

  for (MTLArgument * arg in arguments)
  {
    if ([arg.name compare:kUniformsName] == NSOrderedSame && arg.active && arg.type == MTLArgumentTypeBuffer)
    {
      uniformsBindingIndex = static_cast<int8_t>(arg.index);
    }
    else if (arg.type == MTLArgumentTypeTexture)
    {
      std::string const name([arg.name UTF8String]);
      textureBindingInfo[name].m_textureBindingIndex = static_cast<int8_t>(arg.index);
    }
    else if (arg.type == MTLArgumentTypeSampler)
    {
      std::string const name([arg.name UTF8String]);
      auto const pos = name.find(kSamplerSuffix);
      if (pos == std::string::npos)
        continue;
      std::string const textureName = name.substr(0, pos);
      textureBindingInfo[textureName].m_samplerBindingIndex = static_cast<int8_t>(arg.index);
    }
  }
}
  
MTLVertexDescriptor * GetVertexDescriptor(id<MTLFunction> vertexShader, ProgramInfo::Layout const & layout)
{
  MTLVertexDescriptor * vertexDesc = [[MTLVertexDescriptor alloc] init];
  uint32_t offset = 0;
  uint32_t lastBufferIndex = 0;
  std::map<uint8_t, uint32_t> sizes;
  for (MTLVertexAttribute * attr in vertexShader.vertexAttributes)
  {
    auto const attrIndex = static_cast<uint8_t>(attr.attributeIndex);
    auto const it = layout.find(attrIndex);
    CHECK(it != layout.cend(), ("Invalid layout."));
    auto const bufferIndex = it->second;
    if (lastBufferIndex != bufferIndex)
    {
      offset = 0;
      lastBufferIndex = bufferIndex;
    }
    MTLVertexAttributeDescriptor * attrDesc = vertexDesc.attributes[attr.attributeIndex];
    attrDesc.format = GetFormatByDataType(attr.attributeType);
    auto const sz = GetSizeByDataType(attr.attributeType);
    attrDesc.offset = offset;
    offset += sz;
    sizes[bufferIndex] += sz;
    attrDesc.bufferIndex = bufferIndex;
  }
  
  for (auto const & s : sizes)
    vertexDesc.layouts[s.first].stride = s.second;
  
  return vertexDesc;
}
}  // namespace

std::string DebugPrint(SystemProgram p)
{
  switch (p)
  {
  case SystemProgram::ClearColor: return "ClearColor";
  case SystemProgram::ClearDepth: return "ClearDepth";
  case SystemProgram::ClearColorAndDepth: return "ClearColorAndDepth";
    
  case SystemProgram::SystemProgramsCount:
    CHECK(false, ("Try to output SystemProgramsCount"));
  }
  CHECK(false, ("Unknown program"));
  return {};
}
  
MetalProgramPool::MetalProgramPool(id<MTLDevice> device)
  : m_device(device)
{
  ProgramParams::Init();
  
  NSString * libPath = [[NSBundle mainBundle] pathForResource:@"shaders_metal" ofType:@"metallib"];
  NSError * error = nil;
  m_library = [m_device newLibraryWithFile:libPath error:&error];
  if (error)
  {
    NSLog(@"%@", error);
    CHECK(false, ("Shaders library creation error."));
  }
  m_library.label = @"Shaders library";
}

MetalProgramPool::~MetalProgramPool()
{
  ProgramParams::Destroy();
}

drape_ptr<dp::GpuProgram> MetalProgramPool::GetSystemProgram(SystemProgram program)
{
  auto const & info = kMetalSystemProgramsInfo[static_cast<size_t>(program)];
  return Get(DebugPrint(program), info.m_vertexShaderName, info.m_fragmentShaderName, info.m_layout);
}
  
drape_ptr<dp::GpuProgram> MetalProgramPool::Get(Program program)
{
  auto const & info = kMetalProgramsInfo[static_cast<size_t>(program)];
  return Get(DebugPrint(program), info.m_vertexShaderName, info.m_fragmentShaderName, info.m_layout);
}
  
drape_ptr<dp::GpuProgram> MetalProgramPool::Get(std::string const & programName,
                                                std::string const & vertexShaderName,
                                                std::string const & fragmentShaderName,
                                                std::map<uint8_t, uint8_t> const & layout)
{
  CHECK(!vertexShaderName.empty(), ());
  CHECK(!fragmentShaderName.empty(), ());
  
  id<MTLFunction> vertexShader = GetFunction(vertexShaderName);
  id<MTLFunction> fragmentShader = GetFunction(fragmentShaderName);
  MTLVertexDescriptor * vertexDesc = GetVertexDescriptor(vertexShader, layout);

  // Reflect functions.
  MTLRenderPipelineDescriptor * desc = [[MTLRenderPipelineDescriptor alloc] init];
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
  desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  desc.vertexDescriptor = vertexDesc;
  desc.vertexFunction = vertexShader;
  desc.fragmentFunction = fragmentShader;
  
  NSError * error = nil;
  MTLRenderPipelineReflection * reflectionObj = nil;
  MTLPipelineOption option = MTLPipelineOptionBufferTypeInfo | MTLPipelineOptionArgumentInfo;
  id<MTLRenderPipelineState> pso = [m_device newRenderPipelineStateWithDescriptor:desc
                                                                          options:option
                                                                       reflection:&reflectionObj
                                                                            error:&error];
  if (error != nil || pso == nil)
  {
    NSLog(@"%@", error);
    CHECK(false, ("Failed to create reflection pipeline state."));
  }
  
  int8_t vsUniformsBindingIndex = dp::metal::MetalGpuProgram::kInvalidBindingIndex;
  dp::metal::MetalGpuProgram::TexturesBindingInfo vsTextureBindingInfo;
  GetBindings(reflectionObj.vertexArguments, vsUniformsBindingIndex, vsTextureBindingInfo);
  
  int8_t fsUniformsBindingIndex = dp::metal::MetalGpuProgram::kInvalidBindingIndex;
  dp::metal::MetalGpuProgram::TexturesBindingInfo fsTextureBindingInfo;
  GetBindings(reflectionObj.fragmentArguments, fsUniformsBindingIndex, fsTextureBindingInfo);
  
  return make_unique_dp<dp::metal::MetalGpuProgram>(programName, vertexShader, fragmentShader,
                                                    vsUniformsBindingIndex, fsUniformsBindingIndex,
                                                    std::move(vsTextureBindingInfo),
                                                    std::move(fsTextureBindingInfo),
                                                    vertexDesc);
}
  
id<MTLFunction> MetalProgramPool::GetFunction(std::string const & name)
{
  auto const it = m_functions.find(name);
  if (it == m_functions.end())
  {
    id<MTLFunction> f = [m_library newFunctionWithName:@(name.c_str())];
    CHECK(f != nil, ());
    f.label = [@"Function " stringByAppendingString:@(name.c_str())];
    m_functions.insert(std::make_pair(name, f));
    return f;
  }
  return it->second;
}
}  // namespace metal
}  // namespace gpu
