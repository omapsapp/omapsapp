#include "drape_frontend/arrow3d.hpp"

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/program_manager.hpp"

#include "drape/glsl_func.hpp"
#include "drape/pointers.hpp"
#include "drape/static_texture.hpp"
#include "drape/texture_manager.hpp"

#include "indexer/map_style_reader.hpp"

#include "platform/platform.hpp"

#include "coding/reader.hpp"

#include "geometry/screenbase.hpp"

#include <cstring>
#include <string_view>

#define FAST_OBJ_IMPLEMENTATION
#include "3party/fast_obj/fast_obj.h"

namespace df
{
namespace arrow3d
{
double constexpr kArrowSize = 12.0;
double constexpr kArrow3dScaleMin = 1.0;
double constexpr kArrow3dScaleMax = 2.2;
int constexpr kArrow3dMinZoom = 16;
}  // namespace arrow3d

namespace
{
float constexpr kOutlineScale = 1.2f;

int constexpr kComponentsInVertex = 3;
int constexpr kComponentsInNormal = 3;
int constexpr kComponentsInTexCoord = 2;

df::ColorConstant const kArrow3DShadowColor = "Arrow3DShadow";
df::ColorConstant const kArrow3DObsoleteColor = "Arrow3DObsolete";
df::ColorConstant const kArrow3DColor = "Arrow3D";
df::ColorConstant const kArrow3DOutlineColor = "Arrow3DOutline";

std::string const kDefaultArrowMesh = "arrow.obj";
std::string const kDefaultArrowShadowMesh = "arrow_shadow.obj";

std::string_view const kMainFileId = "main_obj_file_id";

using TLoadingCompletion = std::function<void(
    std::vector<float> positions, std::vector<float> normals, std::vector<float> texCoords)>;
using TLoadingFailure = std::function<void(std::string const &)>;

namespace fast_obj_adapter
{
void * FileOpen(char const * path, void * userData)
{
  // Load only the main OBJ file, skip all the files that can be referred
  // inside the OBJ model.
  if (kMainFileId != path)
    return nullptr;

  return userData;
}

void FileClose(void * file, void * userData)
{
  // Do nothing.
}

size_t FileRead(void * file, void * dst, size_t bytes, void * userData)
{
  auto reader = static_cast<ReaderSource<ReaderPtr<Reader>> *>(userData);
  CHECK(reader != nullptr, ());
  auto const sz = reader->Size();
  if (sz == 0)
    return 0;

  if (bytes > sz)
    bytes = static_cast<size_t>(sz);

  auto const p = reader->Pos();
  reader->Read(dst, bytes);
  CHECK_LESS_OR_EQUAL(p, reader->Pos(), ());
  return static_cast<size_t>(reader->Pos() - p);
}

unsigned long FileSize(void * file, void * userData)
{
  auto reader = static_cast<ReaderSource<ReaderPtr<Reader>> *>(userData);
  CHECK(reader != nullptr, ());
  CHECK_LESS(reader->Size(), static_cast<uint64_t>(std::numeric_limits<size_t>::max()), ());
  return static_cast<size_t>(reader->Size());
}
}  // namespace fast_obj_adapter

class FastObjMeshGuard
{
public:
  explicit FastObjMeshGuard(fastObjMesh * mesh)
    : m_mesh(mesh)
  {
  }

  ~FastObjMeshGuard()
  {
    if (m_mesh)
      fast_obj_destroy(m_mesh);
  }

private:
  fastObjMesh * const m_mesh;
};

bool LoadMesh(std::string const & pathToMesh, bool isDefaultResource,
              TLoadingCompletion const & completionHandler, TLoadingFailure const & failureHandler)
{
  CHECK(completionHandler != nullptr, ());
  CHECK(failureHandler != nullptr, ());

  fastObjMesh * meshData = nullptr;
  try
  {
    ReaderPtr<Reader> reader = isDefaultResource
                                 ? GetStyleReader().GetDefaultResourceReader(pathToMesh)
                                 : GetPlatform().GetReader(pathToMesh);
    ReaderSource source(reader);

    // Read OBJ file.
    fastObjCallbacks callbacks;
    callbacks.file_open = fast_obj_adapter::FileOpen;
    callbacks.file_close = fast_obj_adapter::FileClose;
    callbacks.file_read = fast_obj_adapter::FileRead;
    callbacks.file_size = fast_obj_adapter::FileSize;
    meshData = fast_obj_read_with_callbacks(kMainFileId.data(), &callbacks, &source);
    CHECK(meshData != nullptr, ());
    FastObjMeshGuard guard(meshData);

    // Fill buffers.
    std::vector<float> positions;
    if (meshData->position_count > 1)
      positions.resize(meshData->index_count * kComponentsInVertex);

    std::vector<float> normals;
    if (meshData->normal_count > 1)
      normals.resize(meshData->index_count * kComponentsInNormal);

    std::vector<float> texCoords;
    if (meshData->texcoord_count > 1)
      texCoords.resize(meshData->index_count * kComponentsInTexCoord);

    for (uint32_t i = 0; i < meshData->index_count; ++i)
    {
      if (meshData->position_count > 1)
      {
        memcpy(&positions[i * kComponentsInVertex],
               &meshData->positions[meshData->indices[i].p * kComponentsInVertex],
               sizeof(float) * kComponentsInVertex);
      }

      if (meshData->normal_count > 1)
      {
        memcpy(&normals[i * kComponentsInNormal],
               &meshData->normals[meshData->indices[i].n * kComponentsInNormal],
               sizeof(float) * kComponentsInNormal);
      }

      if (meshData->texcoord_count > 1)
      {
        memcpy(&texCoords[i * kComponentsInTexCoord],
               &meshData->texcoords[meshData->indices[i].t * kComponentsInTexCoord],
               sizeof(float) * kComponentsInTexCoord);
      }
    }

    completionHandler(std::move(positions), std::move(normals), std::move(texCoords));
  }
  catch (RootException & e)
  {
    failureHandler(e.what());
    return false;
  }

  return true;
}
}  // namespace

// static
Arrow3d::PreloadedData Arrow3d::PreloadMesh(std::optional<Arrow3dCustomDecl> const & customDecl,
                                            ref_ptr<dp::TextureManager> texMng)
{
  Arrow3d::PreloadedData data;

  bool const useDefaultResource = !customDecl || customDecl->m_loadFromDefaultResourceFolder;

  // Load arrow mesh.
  auto const & meshPath = customDecl ? customDecl->m_arrowMeshPath : kDefaultArrowMesh;
  data.m_meshData = PreloadedMeshData{};
  if (!LoadMesh(
          meshPath, useDefaultResource,
          [&](std::vector<float> positions, std::vector<float> normals,
              std::vector<float> texCoords)
          {
            if (!positions.empty())
            {
              if (normals.empty())
                normals =
                    dp::MeshObject::GenerateNormalsForTriangles(positions, kComponentsInNormal);

              data.m_meshData->m_positions = std::move(positions);
              data.m_meshData->m_normals = std::move(normals);

              // Texture coordinates.
              ref_ptr<dp::StaticTexture> arrowTexture = texMng->GetArrowTexture();
              CHECK(arrowTexture != nullptr, ("Arrow texture must be initialized before the mesh"));
              // NOTE: texture must be loaded before the mesh.
              if (arrowTexture->IsLoadingCorrect() && !texCoords.empty())
              {
                data.m_arrowMeshTexturingEnabled = true;
                data.m_meshData->m_texCoords = std::move(texCoords);
              }
              else
              {
                data.m_arrowMeshTexturingEnabled = false;
              }
            }
            else
            {
              data.m_meshData.reset();
            }
          },
          [&](std::string const & reason)
          {
            data.m_meshData.reset();
            LOG(LERROR, ("Arrow3D mesh was not loaded:", reason));
          }))
  {
    return {};
  }

  // Load shadow arrow mesh.
  auto const & shadowMeshPath = customDecl ? customDecl->m_shadowMeshPath : kDefaultArrowShadowMesh;
  if (!shadowMeshPath.empty())
  {
    data.m_shadowMeshData = PreloadedMeshData{};
    LoadMesh(
        shadowMeshPath, useDefaultResource,
        [&](std::vector<float> positions, std::vector<float> /* normals */,
            std::vector<float> texCoords)
        {
          // NOTE: Shadow mesh must contain texture coordinates. They're used to create soft shadow.
          if (!positions.empty() && !texCoords.empty())
          {
            data.m_shadowMeshData->m_positions = std::move(positions);
            data.m_shadowMeshData->m_texCoords = std::move(texCoords);
          }
          else
          {
            data.m_shadowMeshData.reset();
          }
        },
        [&](std::string const & reason)
        {
          data.m_shadowMeshData.reset();
          LOG(LWARNING, ("Arrow3D shadow mesh was not loaded:", reason));
        });
  }

  if (customDecl.has_value())
  {
    data.m_texCoordFlipping = glsl::vec2{customDecl->m_flipTexCoordU ? 1.0f : 0.0f,
                                         customDecl->m_flipTexCoordV ? 1.0f : 0.0f};
    data.m_meshOffset =
        glsl::vec3{customDecl->m_offset.x, customDecl->m_offset.y, customDecl->m_offset.z};
    data.m_meshEulerAngles = glsl::vec3{customDecl->m_eulerAngles.x, customDecl->m_eulerAngles.y,
                                        customDecl->m_eulerAngles.z};
    data.m_meshScale =
        glsl::vec3{customDecl->m_scale.x, customDecl->m_scale.y, customDecl->m_scale.z};
    data.m_enableShadow = customDecl->m_enableShadow;
    data.m_enableOutline = customDecl->m_enableOutline;
  }

  return data;
}

Arrow3d::Arrow3d(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> texMng,
                 PreloadedData && preloadedData)
  : m_arrowMesh(context, dp::MeshObject::DrawPrimitive::Triangles, "Arrow3d")
  , m_arrowMeshTexturingEnabled(preloadedData.m_arrowMeshTexturingEnabled)
  , m_texCoordFlipping(std::move(preloadedData.m_texCoordFlipping))
  , m_shadowMesh(
        preloadedData.m_shadowMeshData.has_value()
            ? make_unique_dp<dp::MeshObject>(context, dp::MeshObject::DrawPrimitive::Triangles, "Arrow3dShadow")
            : nullptr)
  , m_state(CreateRenderState(gpu::Program::Arrow3d, DepthLayer::OverlayLayer))
  , m_meshOffset(std::move(preloadedData.m_meshOffset))
  , m_meshEulerAngles(std::move(preloadedData.m_meshEulerAngles))
  , m_meshScale(std::move(preloadedData.m_meshScale))
  , m_enableShadow(preloadedData.m_enableShadow)
  , m_enableOutline(preloadedData.m_enableOutline)
{
  m_state.SetDepthTestEnabled(true);

  // Workaround for OpenGL: some devices require any texture to be set in the rendering pipeline.
  auto const apiVersion = context->GetApiVersion();
  if (apiVersion == dp::ApiVersion::OpenGLES2 || apiVersion == dp::ApiVersion::OpenGLES3)
    m_state.SetColorTexture(texMng->GetSymbolsTexture());

  m_isValid = preloadedData.m_meshData.has_value();

  // Init arrow mesh.
  auto constexpr kVerticesBufferInd = 0;
  auto constexpr kNormalsBufferInd = 1;
  auto constexpr kTexCoordBufferInd = 2;
  if (m_isValid)
  {
    // Positions.
    CHECK(!preloadedData.m_meshData->m_positions.empty(), ());
    m_arrowMesh.SetBuffer(kVerticesBufferInd, std::move(preloadedData.m_meshData->m_positions),
                          sizeof(float) * kComponentsInVertex);
    m_arrowMesh.SetAttribute("a_pos", kVerticesBufferInd, 0 /* offset */, kComponentsInVertex);

    // Normals.
    CHECK(!preloadedData.m_meshData->m_normals.empty(), ());
    m_arrowMesh.SetBuffer(kNormalsBufferInd, std::move(preloadedData.m_meshData->m_normals),
                          sizeof(float) * kComponentsInNormal);
    m_arrowMesh.SetAttribute("a_normal", kNormalsBufferInd, 0 /* offset */, kComponentsInNormal);

    // Texture coordinates.
    if (m_arrowMeshTexturingEnabled)
    {
      CHECK(!preloadedData.m_meshData->m_texCoords.empty(), ());
      m_state.SetColorTexture(texMng->GetArrowTexture());

      m_arrowMesh.SetBuffer(kTexCoordBufferInd, std::move(preloadedData.m_meshData->m_texCoords),
                            sizeof(float) * kComponentsInTexCoord);
      m_arrowMesh.SetAttribute("a_texCoords", kTexCoordBufferInd, 0 /* offset */,
                               kComponentsInTexCoord);
    }
  }

  // Load shadow arrow mesh.
  if (m_isValid && preloadedData.m_shadowMeshData.has_value())
  {
    CHECK(m_shadowMesh != nullptr, ());
    auto constexpr kTexCoordShadowBufferInd = 1;

    // Positions.
    CHECK(!preloadedData.m_shadowMeshData->m_positions.empty(), ());
    m_shadowMesh->SetBuffer(kVerticesBufferInd,
                            std::move(preloadedData.m_shadowMeshData->m_positions),
                            sizeof(float) * kComponentsInVertex);
    m_shadowMesh->SetAttribute("a_pos", kVerticesBufferInd, 0 /* offset */, kComponentsInVertex);

    // Texture coordinates.
    CHECK(!preloadedData.m_shadowMeshData->m_texCoords.empty(), ());
    m_shadowMesh->SetBuffer(kTexCoordShadowBufferInd,
                            std::move(preloadedData.m_shadowMeshData->m_texCoords),
                            sizeof(float) * kComponentsInTexCoord);
    m_shadowMesh->SetAttribute("a_texCoords", kTexCoordShadowBufferInd, 0 /* offset */,
                               kComponentsInTexCoord);
  }
}

bool Arrow3d::IsValid() const { return m_isValid; }

// static
double Arrow3d::GetMaxBottomSize()
{
  double const kBottomSize = 1.0;
  return kBottomSize * arrow3d::kArrowSize * arrow3d::kArrow3dScaleMax * kOutlineScale;
}

void Arrow3d::SetPosition(m2::PointD const & position) { m_position = position; }

void Arrow3d::SetAzimuth(double azimuth)
{
  m_azimuth = azimuth;
}

void Arrow3d::SetPositionObsolete(bool obsolete) { m_obsoletePosition = obsolete; }

void Arrow3d::SetMeshOffset(glsl::vec3 const & offset) { m_meshOffset = offset; }

void Arrow3d::SetMeshRotation(glsl::vec3 const & eulerAngles) { m_meshEulerAngles = eulerAngles; }

void Arrow3d::SetMeshScale(glsl::vec3 const & scale) { m_meshScale = scale; }

void Arrow3d::SetShadowEnabled(bool enabled) { m_enableShadow = enabled; }

void Arrow3d::SetOutlineEnabled(bool enabled) { m_enableOutline = enabled; }

void Arrow3d::Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                     ScreenBase const & screen, bool routingMode)
{
  // Render shadow.
  if (m_shadowMesh && m_enableShadow && screen.isPerspective())
  {
    RenderArrow(context, mng, *m_shadowMesh, screen, gpu::Program::Arrow3dShadow,
                df::GetColorConstant(df::kArrow3DShadowColor), 0.05f /* dz */,
                routingMode ? kOutlineScale : 1.0f /* scaleFactor */);
  }

  // Render outline.
  if (m_shadowMesh && m_enableOutline && routingMode)
  {
    dp::Color const outlineColor = df::GetColorConstant(df::kArrow3DOutlineColor);
    RenderArrow(context, mng, *m_shadowMesh, screen, gpu::Program::Arrow3dOutline, outlineColor,
                0.0f /* dz */, kOutlineScale /* scaleFactor */);
  }

  // Render arrow.
  if (m_arrowMeshTexturingEnabled)
  {
    // Use only alpha channel from arrow color for textured meshes.
    auto const color =
        dp::Color(255, 255, 255,
                  m_obsoletePosition ? df::GetColorConstant(df::kArrow3DObsoleteColor).GetAlpha()
                                     : df::GetColorConstant(df::kArrow3DColor).GetAlpha());

    RenderArrow(context, mng, m_arrowMesh, screen, gpu::Program::Arrow3dTextured, color,
                0.0f /* dz */, 1.0f /* scaleFactor */);
  }
  else
  {
    dp::Color const color =
        df::GetColorConstant(m_obsoletePosition ? df::kArrow3DObsoleteColor : df::kArrow3DColor);
    RenderArrow(context, mng, m_arrowMesh, screen, gpu::Program::Arrow3d, color, 0.0f /* dz */,
                1.0f /* scaleFactor */);
  }
}

void Arrow3d::RenderArrow(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                          dp::MeshObject & mesh, ScreenBase const & screen, gpu::Program program,
                          dp::Color const & color, float dz, float scaleFactor)
{
  gpu::Arrow3dProgramParams params;
  auto [transform, normalTransform] =
      CalculateTransform(screen, dz, scaleFactor, context->GetApiVersion());
  params.m_transform = std::move(transform);
  params.m_normalTransform = std::move(normalTransform);
  params.m_color = glsl::ToVec4(color);
  params.m_texCoordFlipping = m_texCoordFlipping;

  auto gpuProgram = mng->GetProgram(program);
  mesh.Render(context, gpuProgram, m_state, mng->GetParamsSetter(), params);
}

std::pair<glsl::mat4, glsl::mat4> Arrow3d::CalculateTransform(ScreenBase const & screen, float dz,
                                                              float scaleFactor,
                                                              dp::ApiVersion apiVersion) const
{
  double arrowScale = VisualParams::Instance().GetVisualScale() * arrow3d::kArrowSize * scaleFactor;
  if (screen.isPerspective())
  {
    double const t = GetNormalizedZoomLevel(screen.GetScale(), arrow3d::kArrow3dMinZoom);
    arrowScale *= (arrow3d::kArrow3dScaleMin * (1.0 - t) + arrow3d::kArrow3dScaleMax * t);
  }

  glm::quat const qx = glm::angleAxis(m_meshEulerAngles.x, glm::vec3{1.0f, 0.0f, 0.0f});
  glm::quat const qy = glm::angleAxis(m_meshEulerAngles.y, glm::vec3{0.0f, 1.0f, 0.0f});
  glm::quat qz =
      glm::angleAxis(static_cast<float>(m_azimuth + screen.GetAngle() + m_meshEulerAngles.z),
                     glm::vec3{0.0f, 0.0f, -1.0f});
  auto const rotationMatrix = glm::mat4_cast(qz * qy * qx);

  qz = glm::angleAxis(static_cast<float>(m_meshEulerAngles.z), glm::vec3{0.0f, 0.0f, -1.0f});
  auto const normalMatrix = glm::mat4_cast(qz * qy * qx);

  auto const scaleMatrix = glm::scale(
      glm::mat4(1.0f),
      glm::vec3{arrowScale, arrowScale, screen.isPerspective() ? arrowScale : 1.0} * m_meshScale);

  auto const translationMatrix = glm::translate(glm::mat4(1.0f), m_meshOffset);

  auto postProjectionScale =
      glm::vec3{2.0f / screen.PixelRect().SizeX(), 2.0f / screen.PixelRect().SizeY(), 1.0f};
  postProjectionScale.z =
      screen.isPerspective()
          ? std::min(postProjectionScale.x, postProjectionScale.y) * screen.GetScale3d()
          : 0.1f;
  auto const postProjectionScaleMatrix = glm::scale(glm::mat4(1.0f), postProjectionScale);

  m2::PointD const pos = screen.GtoP(m_position);
  auto const dX = static_cast<float>(2.0 * pos.x / screen.PixelRect().SizeX() - 1.0);
  auto const dY = static_cast<float>(2.0 * pos.y / screen.PixelRect().SizeY() - 1.0);
  auto const postProjectionTranslationMatrix =
      glm::translate(glm::mat4(1.0f), glm::vec3{dX, -dY, dz});

  auto modelTransform = postProjectionTranslationMatrix * postProjectionScaleMatrix * scaleMatrix *
                        translationMatrix * rotationMatrix;

  if (screen.isPerspective())
  {
    glm::mat4 pTo3dView = glm::make_mat4x4(screen.Pto3dMatrix().m_data);
    auto postProjectionPerspective = pTo3dView * modelTransform;
    return std::make_pair(postProjectionPerspective, normalMatrix);
  }

  if (apiVersion == dp::ApiVersion::Metal)
  {
    modelTransform[3][2] = modelTransform[3][2] + 0.5f;
    modelTransform[2][2] = modelTransform[2][2] * 0.5f;
  }

  return std::make_pair(modelTransform, normalMatrix);
}
}  // namespace df
