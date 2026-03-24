module glslang.c_interface;

import glslang;

private struct vec(T) {
  T[] data;

  this(ref inout(vec) other) {
    data = other.data.dup;
  }
}

struct input_t {
  EShLanguage stage;
  EShClient client;
  uint client_version;
  EShTargetLanguage target_language;
  uint target_language_version;
  string code;
  int default_version;
  EProfile default_profile;
  int force_default_version_and_profile;
  int forward_compatible;
  EShMessages messages;
  const(TBuiltInResource)* resource;
  glsl_include_callbacks_t callbacks;
  void* callbacks_ctx;
}

struct glsl_include_callbacks_t {
  glsl_include_system_func include_system;
  glsl_include_local_func include_local;
  glsl_free_include_result_func free_include_result;
}

struct glsl_include_result_t {
  string header_name;
  string header_data;
  size_t header_length;
}

alias glsl_include_system_func = glsl_include_result_t* function(
  void* ctx,
  string header_name,
  string includer_name,
  size_t include_depth
);

alias glsl_include_local_func = glsl_include_result_t* function(
  void* ctx,
  string header_name,
  string includer_name,
  size_t include_depth
);

alias glsl_free_include_result_func = int function(
  void* ctx,
  glsl_include_result_t* result
);

struct shader_t {
  TShader shader;
  string preprocessedGLSL;
  string[] baseResourceSetBinding;
}

shader_t shader_create(ref input_t input) {
  shader_t shader;
  
  shader.shader = new TShader(input.stage);
  shader.shader.setStrings(
    [input.code]
  );
  shader.shader.setEnvInput(input.stage, input.client, input.default_version);
  shader.shader.setEnvClient(input.client, input.client_version);
  shader.shader.setEnvTarget(input.target_language, input.target_language_version);

  return shader;
}

bool shader_preprocess(ref shader_t shader, ref input_t input) {
  return shader.shader.preprocess(
    input.resource,
    input.default_version,
    input.default_profile,
    input.force_default_version_and_profile != 0,
    input.forward_compatible != 0,
    input.messages,
    shader.preprocessedGLSL
  );
}
