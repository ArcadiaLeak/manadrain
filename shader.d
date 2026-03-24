import glslangcpp;

import std.stdio;
import std.string;

enum ulong SDL_WINDOW_FULLSCREEN = 0x0000000000000001;    /**< window is in fullscreen mode */
enum ulong SDL_WINDOW_OPENGL = 0x0000000000000002;    /**< window usable with OpenGL context */
enum ulong SDL_WINDOW_OCCLUDED = 0x0000000000000004;    /**< window is occluded */
enum ulong SDL_WINDOW_HIDDEN = 0x0000000000000008;    /**< window is neither mapped onto the desktop nor shown in the taskbar/dock/window list; SDL_ShowWindow() is required for it to become visible */
enum ulong SDL_WINDOW_BORDERLESS = 0x0000000000000010;    /**< no window decoration */
enum ulong SDL_WINDOW_RESIZABLE = 0x0000000000000020;    /**< window can be resized */
enum ulong SDL_WINDOW_MINIMIZED = 0x0000000000000040;    /**< window is minimized */
enum ulong SDL_WINDOW_MAXIMIZED = 0x0000000000000080;    /**< window is maximized */
enum ulong SDL_WINDOW_MOUSE_GRABBED = 0x0000000000000100;    /**< window has grabbed mouse input */
enum ulong SDL_WINDOW_INPUT_FOCUS = 0x0000000000000200;    /**< window has input focus */
enum ulong SDL_WINDOW_MOUSE_FOCUS = 0x0000000000000400;    /**< window has mouse focus */
enum ulong SDL_WINDOW_EXTERNAL = 0x0000000000000800;    /**< window not created by SDL */
enum ulong SDL_WINDOW_MODAL = 0x0000000000001000;    /**< window is modal */
enum ulong SDL_WINDOW_HIGH_PIXEL_DENSITY = 0x0000000000002000;    /**< window uses high pixel density back buffer if possible */
enum ulong SDL_WINDOW_MOUSE_CAPTURE = 0x0000000000004000;    /**< window has mouse captured (unrelated to MOUSE_GRABBED) */
enum ulong SDL_WINDOW_MOUSE_RELATIVE_MODE = 0x0000000000008000;    /**< window has relative mode enabled */
enum ulong SDL_WINDOW_ALWAYS_ON_TOP = 0x0000000000010000;    /**< window should always be above others */
enum ulong SDL_WINDOW_UTILITY = 0x0000000000020000;    /**< window should be treated as a utility window, not showing in the task bar and window list */
enum ulong SDL_WINDOW_TOOLTIP = 0x0000000000040000;    /**< window should be treated as a tooltip and does not get mouse or keyboard focus, requires a parent window */
enum ulong SDL_WINDOW_POPUP_MENU = 0x0000000000080000;    /**< window should be treated as a popup menu, requires a parent window */
enum ulong SDL_WINDOW_KEYBOARD_GRABBED = 0x0000000000100000;    /**< window has grabbed keyboard input */
enum ulong SDL_WINDOW_FILL_DOCUMENT = 0x0000000000200000;    /**< window is in fill-document mode (Emscripten only), since SDL 3.4.0 */
enum ulong SDL_WINDOW_VULKAN = 0x0000000010000000;    /**< window usable for Vulkan surface */
enum ulong SDL_WINDOW_METAL = 0x0000000020000000;    /**< window usable for Metal view */
enum ulong SDL_WINDOW_TRANSPARENT = 0x0000000040000000;    /**< window with transparent buffer */
enum ulong SDL_WINDOW_NOT_FOCUSABLE = 0x0000000080000000;    /**< window should not be focusable */

private bool s_initialized = false;

void glslangInitialize() {
  if (!s_initialized) {
    glslang_initialize_process();
    s_initialized = true;
  }
}

void glslangFinalize() {
  if (s_initialized) {
    glslang_finalize_process();
    s_initialized = false;
  }
}

const(uint[]) compileGLSLtoSPIRV(
  string source,
  string filename,
  bool vertexShader
) {
  if (!s_initialized) {
    throw new Exception("ShaderCompiler not initialized. Call Initialize() first.");
  }
  
  EShLanguage glslangStage = vertexShader ? GLSLANG_STAGE_VERTEX : GLSLANG_STAGE_FRAGMENT;
  glslang_resource_t* default_resource = glslang_default_resource();

  glslang_input_t input = glslang_input_t();
  input.language = GLSLANG_SOURCE_GLSL;
  input.stage = glslangStage;
  input.client = GLSLANG_CLIENT_VULKAN;
  input.client_version = GLSLANG_TARGET_VULKAN_1_0;
  input.target_language = GLSLANG_TARGET_SPV;
  input.target_language_version = GLSLANG_TARGET_SPV_1_0;
  input.code = source.toStringz;
  input.default_version = 310;
  input.default_profile = GLSLANG_ES_PROFILE;
  input.force_default_version_and_profile = false;
  input.forward_compatible = false;
  input.messages = GLSLANG_MSG_DEFAULT_BIT;
  input.resource = default_resource;

  glslang_shader_t* shader = glslang_shader_create(&input);
  if (!shader) {
    throw new Exception("Failed to create shader");
  }

  int parseResult = glslang_shader_preprocess(shader, &input);
  if (!parseResult) {
    string infoLog = glslang_shader_get_info_log(shader).fromStringz.idup;
    string infoDebugLog = glslang_shader_get_info_debug_log(shader).fromStringz.idup;
    
    debug stderr.writef("Shader preprocessing failed for: %s\n", filename);
    debug stderr.writef("Info log: %s\n", infoLog ? infoLog : "No info log");
    debug stderr.writef("Debug log:: %s\n", infoDebugLog ? infoDebugLog : "No debug log");
    
    glslang_shader_delete(shader);
    throw new Exception("Shader preprocessing failed");
  }

  parseResult = glslang_shader_parse(shader, &input);
  if (!parseResult) {
    string infoLog = glslang_shader_get_info_log(shader).fromStringz.idup;
    string infoDebugLog = glslang_shader_get_info_debug_log(shader).fromStringz.idup;
    
    debug stderr.writef("Shader parsing failed for: %s\n", filename);
    debug stderr.writef("Info log: %s\n", infoLog ? infoLog : "No info log");
    debug stderr.writef("Debug log:: %s\n", infoDebugLog ? infoDebugLog : "No debug log");
    
    glslang_shader_delete(shader);
    throw new Exception("Shader parsing failed");
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);
  
  int linkResult = glslang_program_link(
    program,
    GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT
  );
  if (!linkResult) {
    string infoLog = glslang_program_get_info_log(program).fromStringz.idup;
    string infoDebugLog = glslang_program_get_info_debug_log(program).fromStringz.idup;
    
    debug stderr.writef("Shader linking failed for: %s\n", filename);
    debug stderr.writef("Info log: %s\n", infoLog ? infoLog : "No info log");
    debug stderr.writef("Debug log:: %s\n", infoDebugLog ? infoDebugLog : "No debug log");
    
    glslang_program_delete(program);
    glslang_shader_delete(shader);
    throw new Exception("Shader linking failed");
  }

  glslang_program_map_io(program);
  glslang_program_SPIRV_generate(program, input.stage);
  
  size_t spirvSize = glslang_program_SPIRV_get_size(program);
  const uint* spirvCode = glslang_program_SPIRV_get_ptr(program);
  
  if (spirvSize == 0 || spirvCode is null) {
    debug stderr.writef("Failed to generate SPIR-V for: %s\n", filename);

    glslang_program_delete(program);
    glslang_shader_delete(shader);
    throw new Exception("SPIR-V generation failed");
  }

  string spirvMessages = glslang_program_SPIRV_get_messages(program).fromStringz.idup;
  if (spirvMessages && spirvMessages.length > 0) {
    debug writef("SPIR-V messages for %s: %s\n", filename, spirvMessages);
  }
  
  auto spirv = spirvCode[0..spirvSize].dup;

  glslang_program_delete(program);
  glslang_shader_delete(shader);

  return spirv;
}

