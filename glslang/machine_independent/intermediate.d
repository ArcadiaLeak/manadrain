module glslang.machine_independent.intermediate;

import glslang;

import std.container.dlist;
import std.range;

struct TCall {
  this(string pCaller, string pCallee) {
    caller = pCaller;
    callee = pCallee;
    visited = false;
    currentPath = false;
    errorGiven = false;
  }

  string caller;
  string callee;
  bool visited;
  bool currentPath;
  bool errorGiven;
  int calleeBodyPosition;
}

struct TProcesses {
  void addProcess(string process) @safe {
    processes ~= process;
  }

  void addArgument(string arg) @safe {
    processes.back ~= " ";
    processes.back ~= arg;
  }

  const(string[]) getProcesses() const { return processes; }

private:
  string[] processes;
};

class TIntermediate {
  const EShLanguage language;

  string entryPointName;
  string entryPointMangledName;

  DList!TCall callGraph;

  glslang_profile_t profile;
  int version_;
  SpvVersion spvVersion;
  bool useStorageBuffer;

  glslang_source_t source;

  bool originUpperLeft;
  string sourceFile;
  string sourceText;

  TProcesses processes;

  this(
    EShLanguage l, int v = 0,
    glslang_profile_t p = glslang_profile_t.NO_PROFILE
  ) @safe {
    language = l;
  }

  void setSource(glslang_source_t s) @safe { source = s; }
  glslang_source_t getSource() const { return source; }

  void setVersion(int v) { version_ = v; }
  int getVersion() const { return profile; }

  void setProfile(glslang_profile_t p) { profile = p; }
  glslang_profile_t getProfile() const { return profile; }

  void setSourceFile(string file) { if (file != null) sourceFile = file; }
  string getSourceFile() const { return sourceFile; }

  void addSourceText(string text) { sourceText ~= text; }
  string getSourceText() const { return sourceText; }

  void setOriginUpperLeft() { originUpperLeft = true; }
  bool getOriginUpperLeft() const { return originUpperLeft; }

  void setEntryPointName(string ep) @safe {
    entryPointName = ep;
    processes.addProcess("entry-point");
    processes.addArgument(entryPointName);
  }

  void setUseStorageBuffer() @safe { useStorageBuffer = true; }

  void setSpv(in SpvVersion s) {
    spvVersion = s;

    if (spvVersion.vulkan > 0)
      processes.addProcess("client vulkan100");
    if (spvVersion.openGl > 0)
      processes.addProcess("client opengl100");

    switch (spvVersion.spv) {
      case 0:
        break;
      case glslang_target_language_version_t.TARGET_SPV_1_0:
        break;
      case glslang_target_language_version_t.TARGET_SPV_1_1:
        processes.addProcess("target-env spirv1.1");
        break;
      case glslang_target_language_version_t.TARGET_SPV_1_2:
        processes.addProcess("target-env spirv1.2");
        break;
      case glslang_target_language_version_t.TARGET_SPV_1_3:
        processes.addProcess("target-env spirv1.3");
        break;
      case glslang_target_language_version_t.TARGET_SPV_1_4:
        processes.addProcess("target-env spirv1.4");
        break;
      case glslang_target_language_version_t.TARGET_SPV_1_5:
        processes.addProcess("target-env spirv1.5");
        break;
      case glslang_target_language_version_t.TARGET_SPV_1_6:
        processes.addProcess("target-env spirv1.6");
        break;
      default:
        processes.addProcess("target-env spirvUnknown");
        break;
    }

    switch (spvVersion.vulkan) {
      case 0:
        break;
      case glslang_target_client_version_t.TARGET_VULKAN_1_0:
        processes.addProcess("target-env vulkan1.0");
        break;
      case glslang_target_client_version_t.TARGET_VULKAN_1_1:
        processes.addProcess("target-env vulkan1.1");
        break;
      case glslang_target_client_version_t.TARGET_VULKAN_1_2:
        processes.addProcess("target-env vulkan1.2");
        break;
      case glslang_target_client_version_t.TARGET_VULKAN_1_3:
        processes.addProcess("target-env vulkan1.3");
        break;
      case glslang_target_client_version_t.TARGET_VULKAN_1_4:
        processes.addProcess("target-env vulkan1.4");
        break;
      default:
        processes.addProcess("target-env vulkanUnknown");
        break;
    }

    if (spvVersion.openGl > 0)
      processes.addProcess("target-env opengl");
  }

  void addProcess(string process) { processes.addProcess = process; }
  void addProcessArgument(string arg) { processes.addArgument = arg; }
}
