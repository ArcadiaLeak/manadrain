module glslang.c_shader_types;

struct EShMessages {
  bool MSG_RELAXED_ERRORS_BIT;
  bool MSG_SUPPRESS_WARNINGS_BIT;
  bool MSG_AST_BIT;
  bool MSG_SPV_RULES_BIT;
  bool MSG_VULKAN_RULES_BIT;
  bool MSG_ONLY_PREPROCESSOR_BIT;
  bool MSG_CASCADING_ERRORS_BIT;
  bool MSG_KEEP_UNCALLED_BIT;
  bool MSG_DEBUG_INFO_BIT;
  bool MSG_BUILTIN_SYMBOL_TABLE_BIT;
  bool MSG_ENHANCED;
  bool MSG_ABSOLUTE_PATH;
  bool MSG_DISPLAY_ERROR_COLUMN;
  bool MSG_LINK_TIME_OPTIMIZATION_BIT;
  bool MSG_VALIDATE_CROSS_STAGE_IO_BIT;
}

enum uint TARGET_SPV_1_0 = (1 << 16);
enum uint TARGET_SPV_1_1 = (1 << 16) | (1 << 8);
enum uint TARGET_SPV_1_2 = (1 << 16) | (2 << 8);
enum uint TARGET_SPV_1_3 = (1 << 16) | (3 << 8);
enum uint TARGET_SPV_1_4 = (1 << 16) | (4 << 8);
enum uint TARGET_SPV_1_5 = (1 << 16) | (5 << 8);
enum uint TARGET_SPV_1_6 = (1 << 16) | (6 << 8);

enum EShTargetLanguage {
  TARGET_NONE,
  TARGET_SPV
}

enum uint TARGET_VULKAN_1_0 = (1 << 22);
enum uint TARGET_VULKAN_1_1 = (1 << 22) | (1 << 12);
enum uint TARGET_VULKAN_1_2 = (1 << 22) | (2 << 12);
enum uint TARGET_VULKAN_1_3 = (1 << 22) | (3 << 12);
enum uint TARGET_VULKAN_1_4 = (1 << 22) | (4 << 12);
enum uint TARGET_OPENGL_450 = 450;

enum EShClient {
  CLIENT_NONE,
  CLIENT_VULKAN,
  CLIENT_OPENGL
}

enum EShLanguage {
  STAGE_VERTEX,
  STAGE_TESSCONTROL,
  STAGE_TESSEVALUATION,
  STAGE_GEOMETRY,
  STAGE_FRAGMENT,
  STAGE_COMPUTE,
  STAGE_RAYGEN,
  STAGE_INTERSECT,
  STAGE_ANYHIT,
  STAGE_CLOSESTHIT,
  STAGE_MISS,
  STAGE_CALLABLE,
  STAGE_TASK,
  STAGE_MESH
}
