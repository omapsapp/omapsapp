#include "shaders/program_params.hpp"

namespace gpu
{
// static
std::map<std::string, std::string> ProgramParams::m_boundParams;

// static
void ProgramParams::Init()
{
  MapProgramParams::BindPrograms(m_boundParams);
  RouteProgramParams::BindPrograms(m_boundParams);
  TrafficProgramParams::BindPrograms(m_boundParams);
  TransitProgramParams::BindPrograms(m_boundParams);
  GuiProgramParams::BindPrograms(m_boundParams);
  ShapesProgramParams::BindPrograms(m_boundParams);
  Arrow3dProgramParams::BindPrograms(m_boundParams);
  DebugRectProgramParams::BindPrograms(m_boundParams);
  ScreenQuadProgramParams::BindPrograms(m_boundParams);
  SMAAProgramParams::BindPrograms(m_boundParams);
  ImGuiProgramParams::BindPrograms(m_boundParams);
}

// static
void ProgramParams::Destroy()
{
  m_boundParams.clear();
}

// static
std::string ProgramParams::GetBoundParamsName(ref_ptr<dp::GpuProgram> program)
{
  auto const it = m_boundParams.find(program->GetName());
  ASSERT(it != m_boundParams.cend(), (program->GetName(), "Program is not bound to params"));
  if (it == m_boundParams.cend())
    return {};
  return it->second;
}
}  // namespace gpu
