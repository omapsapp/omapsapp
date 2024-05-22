#include "drape_frontend/gui/shape.hpp"

#include "drape_frontend/visual_params.hpp"

#include "drape/glsl_func.hpp"
#include "drape/utils/projection.hpp"

#include "base/logging.hpp"

#include <algorithm>
#include <array>

namespace gui
{
Handle::Handle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot, m2::PointF const & size)
  : dp::OverlayHandle(dp::OverlayID(FeatureID(MwmSet::MwmId{}, id)), anchor, 0 /* priority */,
                      1 /* minVisibleScale */, false /* isBillboard */)
  , m_pivot(glsl::ToVec2(pivot))
  , m_size(size)
{}

bool Handle::Update(ScreenBase const &)
{
  using namespace glsl;

  if (IsVisible())
  {
    m_params.m_modelView = transpose(translate(mat4(), vec3(m_pivot, 0.0)));
    auto const & params = df::VisualParams::Instance().GetGlyphVisualParams();
    m_params.m_contrastGamma = glsl::vec2(params.m_guiContrast, params.m_guiGamma);
    m_params.m_isOutlinePass = 0.0f;
    m_params.m_opacity = 1.0f;
  }

  return true;
}

bool Handle::IndexesRequired() const
{
  return false;
}

m2::RectD Handle::GetPixelRect(ScreenBase const &, bool perspective) const
{
  // There is no need to check intersection of gui elements.
  UNUSED_VALUE(perspective);
  return {};
}

void Handle::GetPixelShape(ScreenBase const & screen, bool perspective,
                           dp::OverlayHandle::Rects & rects) const
{
  // There is no need to check intersection of gui elements.
  UNUSED_VALUE(screen);
  UNUSED_VALUE(rects);
  UNUSED_VALUE(perspective);
}

bool TappableHandle::IsTapped(m2::RectD const & touchArea) const
{
  if (!IsVisible())
    return false;

  if (m_anchor == dp::Anchor::Center)
  {
    m2::RectD rect(m_pivot.x - m_size.x * 0.5, m_pivot.y - m_size.y * 0.5,
                   m_pivot.x + m_size.x * 0.5, m_pivot.y + m_size.y * 0.5);
    return rect.Intersect(touchArea);
  }
  else
  {
    LOG(LWARNING, ("Tapping on an overlay is not supported. Anchor type = ", m_anchor));
  }

  return false;
}

ShapeRenderer::~ShapeRenderer()
{
  ForEachShapeInfo([](ShapeControl::ShapeInfo & info) { info.Destroy(); });
}

void ShapeRenderer::Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng)
{
  ForEachShapeInfo([context, mng](ShapeControl::ShapeInfo & info) mutable
  {
    info.m_buffer->Build(context, mng->GetProgram(info.m_state.GetProgram<gpu::Program>()));
  });
}

void ShapeRenderer::Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng,
                           ScreenBase const & screen)
{
  m2::RectD const & pxRect = screen.PixelRectIn3d();
  auto m = dp::MakeProjection(context->GetApiVersion(), 0.0f, static_cast<float>(pxRect.SizeX()),
                              static_cast<float>(pxRect.SizeY()), 0.0f);
  glsl::mat4 const projection = glsl::make_mat4(m.data());

  ForEachShapeInfo([&projection, &screen, context, mng](ShapeControl::ShapeInfo & info) mutable
  {
    if (!info.m_handle->Update(screen))
      return;

    if (!info.m_handle->IsVisible())
      return;

    ref_ptr<dp::GpuProgram> prg = mng->GetProgram(info.m_state.GetProgram<gpu::Program>());
    prg->Bind();
    dp::ApplyState(context, prg, info.m_state);

    auto params = info.m_handle->GetParams();
    params.m_projection = projection;
    mng->GetParamsSetter()->Apply(context, prg, params);

    if (info.m_handle->HasDynamicAttributes())
    {
      dp::AttributeBufferMutator mutator;
      ref_ptr<dp::AttributeBufferMutator> mutatorRef = make_ref(&mutator);
      info.m_handle->GetAttributeMutation(mutatorRef);
      info.m_buffer->ApplyMutation(context, nullptr, mutatorRef);
    }

    info.m_buffer->Render(context, info.m_state.GetDrawAsLine());
  });
}

void ShapeRenderer::AddShape(dp::RenderState const & state, drape_ptr<dp::RenderBucket> && bucket)
{
  m_shapes.emplace_back(ShapeControl());
  m_shapes.back().AddShape(state, std::move(bucket));
}

void ShapeRenderer::AddShapeControl(ShapeControl && control)
{
  m_shapes.push_back(std::move(control));
}

void ShapeRenderer::SetPivot(m2::PointF const & pivot)
{
  for (auto & control : m_shapes)
  {
    for (auto & info : control.m_shapesInfo)
      info.m_handle->SetPivot(glsl::ToVec2(pivot));
  }
}

void ShapeRenderer::ForEachShapeControl(TShapeControlEditFn const & fn)
{
  std::for_each(m_shapes.begin(), m_shapes.end(), fn);
}

void ShapeRenderer::ForEachShapeInfo(ShapeRenderer::TShapeInfoEditFn const & fn)
{
  ForEachShapeControl([&fn](ShapeControl & shape)
  {
    std::for_each(shape.m_shapesInfo.begin(), shape.m_shapesInfo.end(), fn);
  });
}

ref_ptr<Handle> ShapeRenderer::ProcessTapEvent(m2::RectD const & touchArea)
{
  ref_ptr<Handle> resultHandle = nullptr;
  ForEachShapeInfo([&resultHandle, &touchArea](ShapeControl::ShapeInfo & shapeInfo)
  {
    if (shapeInfo.m_handle->IsTapped(touchArea))
      resultHandle = make_ref(shapeInfo.m_handle);
  });

  return resultHandle;
}

ref_ptr<Handle> ShapeRenderer::FindHandle(FeatureID const & id)
{
  ref_ptr<Handle> resultHandle = nullptr;
  ForEachShapeInfo([&resultHandle, &id](ShapeControl::ShapeInfo & shapeInfo)
  {
    if (shapeInfo.m_handle->GetOverlayID().m_featureId == id)
      resultHandle = make_ref(shapeInfo.m_handle);
  });

  return resultHandle;
}

ShapeControl::ShapeInfo::ShapeInfo(dp::RenderState const & state,
                                   drape_ptr<dp::VertexArrayBuffer> && buffer,
                                   drape_ptr<Handle> && handle)
  : m_state(state)
  , m_buffer(std::move(buffer))
  , m_handle(std::move(handle))
{}

void ShapeControl::ShapeInfo::Destroy()
{
  m_handle.reset();
  m_buffer.reset();
}

void ShapeControl::AddShape(dp::RenderState const & state, drape_ptr<dp::RenderBucket> && bucket)
{
  ASSERT(bucket->GetOverlayHandlesCount() == 1, ());

  drape_ptr<dp::OverlayHandle> handle = bucket->PopOverlayHandle();
  ASSERT(dynamic_cast<Handle *>(handle.get()), ());

  m_shapesInfo.emplace_back(state, std::move(bucket->MoveBuffer()),
                            drape_ptr<Handle>(static_cast<Handle *>(handle.release())));
}
}  // namespace gui
