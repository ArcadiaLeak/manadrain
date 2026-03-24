import std.string;
import std.stdio;

static import freetype;
static import yoga;
import renderer;
import sdl;
import shader;

SDL_GPUGraphicsPipeline* createGraphicsPipeline(
  SDL_GPUDevice* device,
  SDL_Window* window
) {
  glslangInitialize();
  scope(exit) glslangFinalize();

  string vertexShaderSource =
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

  auto vertShaderCode = compileGLSLtoSPIRV(
    vertexShaderSource,
    "cube.vert",
    true
  );
  auto vertexShader = () {
    auto vertMetadata = SDL_ShaderCross_ReflectGraphicsSPIRV(
      cast(const(ubyte)*) vertShaderCode,
      vertShaderCode.length * uint.sizeof,
      0
    );
    scope(exit) SDL_free(vertMetadata);
    auto vertSpirvInfo = SDL_ShaderCross_SPIRV_Info(
      cast(const(ubyte)*) vertShaderCode,
      vertShaderCode.length * uint.sizeof,
      "main",
      SDL_ShaderCross_ShaderStage.SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
      0
    );
    return SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
      device,
      &vertSpirvInfo,
      &vertMetadata.resource_info,
      0
    );
  }();
  scope(exit) SDL_ReleaseGPUShader(device, vertexShader);

  string fragmentShaderSource =
    "#version 310 es\n" ~
    "precision highp float;\n" ~
    "\n" ~
    "layout(location = 0) out vec4 outColor;\n" ~
    "\n" ~
    "void main() {\n" ~
    "  outColor = vec4(1.0, 0.5, 0.2, 1.0);\n" ~
    "}\n";

  auto fragShaderCode = compileGLSLtoSPIRV(
    fragmentShaderSource,
    "cube.frag",
    false
  );
  auto fragShader = () {
    auto fragMetadata = SDL_ShaderCross_ReflectGraphicsSPIRV(
      cast(const(ubyte)*) fragShaderCode,
      fragShaderCode.length * uint.sizeof,
      0
    );
    scope(exit) SDL_free(fragMetadata);
    auto fragSpirvInfo = SDL_ShaderCross_SPIRV_Info(
      cast(const(ubyte)*) fragShaderCode,
      fragShaderCode.length * uint.sizeof,
      "main",
      SDL_ShaderCross_ShaderStage.SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
      0
    );
    return SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
      device,
      &fragSpirvInfo,
      &fragMetadata.resource_info,
      0
    );
  }();
  scope(exit) SDL_ReleaseGPUShader(device, fragShader);

  SDL_GPUColorTargetDescription colorTargetDescription;
  colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(device, window);

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo;

  SDL_GPUVertexBufferDescription vertexBufferDescription;
  vertexBufferDescription.slot = 0;
  vertexBufferDescription.input_rate = SDL_GPUVertexInputRate.SDL_GPU_VERTEXINPUTRATE_VERTEX;
  vertexBufferDescription.instance_step_rate = 0;
  vertexBufferDescription.pitch = Vertex.sizeof;

  SDL_GPUVertexAttribute vertexAttribute;
  vertexAttribute.buffer_slot = 0;
  vertexAttribute.format = SDL_GPUVertexElementFormat.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  vertexAttribute.location = 0;
  vertexAttribute.offset = Vertex.position.offsetof;

  pipelineCreateInfo.target_info.num_color_targets = 1;
  pipelineCreateInfo.target_info.color_target_descriptions = &colorTargetDescription;

  pipelineCreateInfo.vertex_input_state.num_vertex_buffers = 1;
  pipelineCreateInfo.vertex_input_state.vertex_buffer_descriptions =
    &vertexBufferDescription;
  pipelineCreateInfo.vertex_input_state.num_vertex_attributes = 1;
  pipelineCreateInfo.vertex_input_state.vertex_attributes = &vertexAttribute;

  pipelineCreateInfo.primitive_type =
    SDL_GPUPrimitiveType.SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipelineCreateInfo.vertex_shader = vertexShader;
  pipelineCreateInfo.fragment_shader = fragShader;

  pipelineCreateInfo.rasterizer_state.fill_mode =
    SDL_GPUFillMode.SDL_GPU_FILLMODE_FILL;
  
  return SDL_CreateGPUGraphicsPipeline(
    device,
    &pipelineCreateInfo
  );
};

void main() {
  SDL_Init(SDL_INIT_VIDEO);
  scope(exit) SDL_Quit();

  SDL_GPUDevice* device = SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV,
		true,
		null
  );
  scope(exit) SDL_DestroyGPUDevice(device);

  SDL_Window* window = SDL_CreateWindow(
    "Heuristic Graphic",
    800,
    600,
    SDL_WINDOW_RESIZABLE
  );
  scope(exit) SDL_DestroyWindow(window);

  SDL_ClaimWindowForGPUDevice(device, window);
  scope(exit) SDL_ReleaseWindowFromGPUDevice(device, window);

  auto pipeline = createGraphicsPipeline(device, window);
  scope(exit) SDL_ReleaseGPUGraphicsPipeline(device, pipeline);

  SDL_Event event;
  bool quit = false;

  Renderer renderer = Renderer(
    window,
    device,
    pipeline
  );
  
  auto timeLast = SDL_GetPerformanceCounter();

  while (!quit) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        quit = true;
      }
    }

    auto timeCurr = SDL_GetPerformanceCounter();
    scope(exit) timeLast = timeCurr;

    auto deltaTime = cast(float) (timeCurr - timeLast) / 1e9;
    renderer.update(deltaTime);
    renderer.drawFrame();
  }
}
