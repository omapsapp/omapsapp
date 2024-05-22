#include "testing/testing.hpp"

#include "drape/drape_global.hpp"
#include "drape/gl_gpu_program.hpp"
#include "drape/uniform_value.hpp"

#include "drape/drape_tests/gl_mock_functions.hpp"

#include <cstring>
#include <string>
#include <utility>

#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;
using ::testing::AnyOf;
using ::testing::IgnoreResult;
using ::testing::Invoke;
using ::testing::InSequence;
using namespace dp;

namespace uniform_value_tests
{
template<typename T>
class MemoryComparer
{
public:
  MemoryComparer(T const * memory, uint32_t size)
    : m_result(false)
    , m_memory(memory)
    , m_size(size)
  {}

  void Compare(int32_t /* id */, T const * memory)
  {
    m_result = memcmp(m_memory, memory, m_size) == 0;
  }

  bool GetResult() const
  {
    return m_result;
  }

private:
  bool m_result;
  T const * m_memory;
  uint32_t m_size;
};

void mock_glGetActiveUniform(uint32_t /* programID */, uint32_t index, int32_t * size,
                             glConst * type, std::string & name)
{
  *size = 1;
  if (index < 9)
  {
    static std::pair<std::string, glConst> mockUniforms[9] =
    {
      {"position0", gl_const::GLIntType},
      {"position1", gl_const::GLIntVec2},
      {"position2", gl_const::GLIntVec3},
      {"position3", gl_const::GLIntVec4},
      {"position4", gl_const::GLFloatType},
      {"position5", gl_const::GLFloatVec2},
      {"position6", gl_const::GLFloatVec3},
      {"position7", gl_const::GLFloatVec4},
      {"viewModel", gl_const::GLFloatMat4}
    };
    name = mockUniforms[index].first;
    *type = mockUniforms[index].second;
  }
  else
  {
    ASSERT(false, ("Undefined index:", index));
  }
}

UNIT_TEST(UniformValueTest)
{
  uint32_t constexpr VertexShaderID = 1;
  uint32_t constexpr FragmentShaderID = 2;
  uint32_t constexpr ProgramID = 2;
  int32_t constexpr positionLoc = 10;
  int32_t constexpr modelViewLoc = 11;

  float matrix[16] =
  {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };

  MemoryComparer<float> comparer(matrix, 16 * sizeof(float));

  {
    InSequence seq;
    EXPECTGL(glCreateShader(gl_const::GLVertexShader)).WillOnce(Return(VertexShaderID));
    EXPECTGL(glShaderSource(VertexShaderID, _)).Times(1);
    EXPECTGL(glCompileShader(VertexShaderID, _)).WillOnce(Return(true));
    EXPECTGL(glCreateShader(gl_const::GLFragmentShader)).WillOnce(Return(FragmentShaderID));
    EXPECTGL(glShaderSource(FragmentShaderID, _)).Times(1);
    EXPECTGL(glCompileShader(FragmentShaderID, _)).WillOnce(Return(true));

    EXPECTGL(glCreateProgram()).WillOnce(Return(ProgramID));
    EXPECTGL(glAttachShader(ProgramID, VertexShaderID));
    EXPECTGL(glAttachShader(ProgramID, FragmentShaderID));

    EXPECTGL(glLinkProgram(ProgramID, _)).WillOnce(Return(true));

    EXPECTGL(glGetProgramiv(ProgramID, gl_const::GLActiveUniforms)).WillOnce(Return(9));
    for (int i = 0; i < 9; i++)
    {
      EXPECTGL(glGetActiveUniform(ProgramID, _, _, _, _)).WillOnce(Invoke(mock_glGetActiveUniform));
      EXPECTGL(glGetUniformLocation(ProgramID, _)).WillOnce(Return(i == 8 ? modelViewLoc : positionLoc));
    }

    EXPECTGL(glDetachShader(ProgramID, VertexShaderID));
    EXPECTGL(glDetachShader(ProgramID, FragmentShaderID));

    EXPECTGL(glUseProgram(ProgramID));

    EXPECTGL(glUniformValuei(positionLoc, 1));

    EXPECTGL(glUniformValuei(positionLoc, 1, 2));

    EXPECTGL(glUniformValuei(positionLoc, 1, 2, 3));

    EXPECTGL(glUniformValuei(positionLoc, 1, 2, 3, 4));

    EXPECTGL(glUniformValuef(positionLoc, 1.0f));

    EXPECTGL(glUniformValuef(positionLoc, 1.0f, 2.0f));

    EXPECTGL(glUniformValuef(positionLoc, 1.0f, 2.0f, 3.0f));

    EXPECTGL(glUniformValuef(positionLoc, 1.0f, 2.0f, 3.0f, 4.0f));

    EXPECTGL(glUniformMatrix4x4Value(modelViewLoc, _))
        .WillOnce(Invoke(&comparer, &MemoryComparer<float>::Compare));

    EXPECTGL(glUseProgram(0));
    EXPECTGL(glDeleteProgram(ProgramID));
    EXPECTGL(glDeleteShader(AnyOf(VertexShaderID, FragmentShaderID))).Times(2);
  }

  drape_ptr<Shader> vs = make_unique_dp<Shader>("", "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }",
                                                "", Shader::Type::VertexShader);

  drape_ptr<Shader> fs = make_unique_dp<Shader>("", "void main() { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); }",
                                                "", Shader::Type::FragmentShader);

  drape_ptr<GLGpuProgram> program = make_unique_dp<GLGpuProgram>("", make_ref(vs), make_ref(fs));

  program->Bind();

  UniformValue::ApplyRaw(positionLoc, 1);

  UniformValue::ApplyRaw(positionLoc, glsl::ivec2(1, 2));

  UniformValue::ApplyRaw(positionLoc, glsl::ivec3(1, 2, 3));

  UniformValue::ApplyRaw(positionLoc, glsl::ivec4(1, 2, 3, 4));

  UniformValue::ApplyRaw(positionLoc, 1.0f);

  UniformValue::ApplyRaw(positionLoc, glsl::vec2(1.0f, 2.0f));

  UniformValue::ApplyRaw(positionLoc, glsl::vec3(1.0f, 2.0f, 3.0f));

  UniformValue::ApplyRaw(positionLoc, glsl::vec4(1.0f, 2.0f, 3.0f, 4.0f));

  UniformValue::ApplyRaw(modelViewLoc, glsl::make_mat4(matrix));

  program->Unbind();

  program.reset();
  vs.reset();
  fs.reset();
}
}  // namespace uniform_value_tests
