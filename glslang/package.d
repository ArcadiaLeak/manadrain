module glslang;

public import glslang.include;
public import glslang.machine_independent;
public import glslang.resource_limits;

public import glslang.c_interface;
public import glslang.c_shader_types;

void main() {
  EShLanguage stage = EShLanguage.STAGE_VERTEX;
  const(TBuiltInResource)* default_resource = GetDefaultResources;
  
  string vertexShaderCode =
    "#version 310 es\n" ~
    "precision highp float;\n" ~
    "\n" ~
    "layout(location = 0) in vec3 inPosition;\n" ~
    "\n" ~
    "layout(std140, set = 1, binding = 0) uniform UniformBufferObject {\n" ~
    "  mat4 model;\n" ~
    "  mat4 view;\n" ~
    "  mat4 proj;\n" ~
    "} ubo;\n" ~
    "\n" ~
    "void main() {\n" ~
    "  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);\n" ~
    "}\n";

  input_t input;
  input.language = source_t.SOURCE_GLSL;
  input.stage = stage;
  input.client = client_t.CLIENT_VULKAN;
  input.client_version = target_client_version_t.TARGET_VULKAN_1_0;
  input.target_language = target_language_t.TARGET_SPV;
  input.target_language_version = target_language_version_t.TARGET_SPV_1_0;
  input.code = vertexShaderCode;
  input.default_version = 310;
  input.default_profile = profile_t.ES_PROFILE;
  input.force_default_version_and_profile = false;
  input.forward_compatible = false;
  input.messages = messages_t.MSG_DEFAULT_BIT;
  input.resource = default_resource;

  shader_t shader = input.shader_create;
  shader.shader_preprocess = input;

  import std.stdio;
  writeln(shader.preprocessedGLSL);
}
