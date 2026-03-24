import std.stdio;
import std.algorithm.mutation;
static import core.stdc.string;

import sdl;
import shader;
static import mat4;

const int MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex {
  float[3] position;
}

struct UniformBufferObject {
  align(16) float[4][4] model;
  align(16) float[4][4] view;
  align(16) float[4][4] proj;
}

struct Renderer {
  static immutable Vertex[] s_vertices = [
    Vertex([-0.5f, -0.5f,  0.5f]), 
    Vertex([ 0.5f, -0.5f,  0.5f]), 
    Vertex([ 0.5f,  0.5f,  0.5f]), 
    Vertex([-0.5f,  0.5f,  0.5f]), 
    
    Vertex([-0.5f, -0.5f, -0.5f]), 
    Vertex([ 0.5f, -0.5f, -0.5f]), 
    Vertex([ 0.5f,  0.5f, -0.5f]), 
    Vertex([-0.5f,  0.5f, -0.5f]), 
    
    Vertex([-0.5f, -0.5f, -0.5f]), 
    Vertex([-0.5f, -0.5f,  0.5f]), 
    Vertex([-0.5f,  0.5f,  0.5f]), 
    Vertex([-0.5f,  0.5f, -0.5f]), 
    
    Vertex([ 0.5f, -0.5f,  0.5f]), 
    Vertex([ 0.5f, -0.5f, -0.5f]), 
    Vertex([ 0.5f,  0.5f, -0.5f]), 
    Vertex([ 0.5f,  0.5f,  0.5f]), 
    
    Vertex([-0.5f,  0.5f,  0.5f]), 
    Vertex([ 0.5f,  0.5f,  0.5f]), 
    Vertex([ 0.5f,  0.5f, -0.5f]), 
    Vertex([-0.5f,  0.5f, -0.5f]), 
    
    Vertex([-0.5f, -0.5f, -0.5f]), 
    Vertex([ 0.5f, -0.5f, -0.5f]), 
    Vertex([ 0.5f, -0.5f,  0.5f]), 
    Vertex([-0.5f, -0.5f,  0.5f]), 
  ];

  static immutable ushort[] s_indices = [
    0, 1, 2, 2, 3, 0,    
    4, 7, 6, 6, 5, 4,    
    8, 9, 10, 10, 11, 8,  
    12, 13, 14, 14, 15, 12, 
    16, 17, 18, 18, 19, 16, 
    20, 21, 22, 22, 23, 20  
  ];

  private {
    SDL_Window* m_window;

    SDL_GPUDevice* m_device;
    SDL_GPUGraphicsPipeline* m_pipeline;

    SDL_GPUBuffer* m_vertexBuffer;
    SDL_GPUBuffer* m_indexBuffer;

    float m_rotationAngle = 0.0f;
    float[3] m_rotationAxis = [0.0f, 1.0f, 0.5f];
  }

  this(
    SDL_Window* window,
    SDL_GPUDevice* device,
    SDL_GPUGraphicsPipeline* pipeline

  ) {
    m_window = window;
    m_device = device;
    m_pipeline = pipeline;

    auto vertexBufferCreateInfo = SDL_GPUBufferCreateInfo(
      SDL_GPU_BUFFERUSAGE_VERTEX,
      Vertex.sizeof * s_vertices.length
    );
    m_vertexBuffer = SDL_CreateGPUBuffer(
      device,
      &vertexBufferCreateInfo
    );

    auto indexBufferCreateInfo = SDL_GPUBufferCreateInfo(
      SDL_GPU_BUFFERUSAGE_INDEX,
      ushort.sizeof * s_indices.length
    );
    m_indexBuffer = SDL_CreateGPUBuffer(
      device,
      &indexBufferCreateInfo
    );

    auto transferBufferCreateInfo = SDL_GPUTransferBufferCreateInfo(
      SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      vertexBufferCreateInfo.size + indexBufferCreateInfo.size
    );
    auto transferBuffer = SDL_CreateGPUTransferBuffer(
      m_device,
      &transferBufferCreateInfo
    );
    scope(exit) SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);

    {
      ubyte* transferData = cast(ubyte*) SDL_MapGPUTransferBuffer(
        m_device,
        transferBuffer,
        false
      );
      scope(exit) SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);

      core.stdc.string.memcpy(
        transferData,
        s_vertices.ptr,
        vertexBufferCreateInfo.size
      );
      core.stdc.string.memcpy(
        transferData + vertexBufferCreateInfo.size,
        s_indices.ptr,
        indexBufferCreateInfo.size
      );
    }
    
    auto uploadCmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
		scope(exit) SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

		auto copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
		scope(exit) SDL_EndGPUCopyPass(copyPass);

    {
      auto bufferLocation = SDL_GPUTransferBufferLocation(
        transferBuffer,
        0
      );
      auto bufferRegion = SDL_GPUBufferRegion(
        m_vertexBuffer,
        0,
        vertexBufferCreateInfo.size
      );

      SDL_UploadToGPUBuffer(
        copyPass,
        &bufferLocation,
        &bufferRegion,
        false
      );
    }

    {
      auto bufferLocation = SDL_GPUTransferBufferLocation(
        transferBuffer,
        vertexBufferCreateInfo.size
      );
      auto bufferRegion = SDL_GPUBufferRegion(
        m_indexBuffer,
        0,
        indexBufferCreateInfo.size
      );

      SDL_UploadToGPUBuffer(
        copyPass,
        &bufferLocation,
        &bufferRegion,
        false
      );
    }
  }

  ~this() {
    SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
	  SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
  }

  void update(float deltaTime) {
    m_rotationAngle += deltaTime * 45.0f;
    if (m_rotationAngle >= 360.0f) {
      m_rotationAngle -= 360.0f;
    }
  }

  UniformBufferObject computeUniformMatrices() {
    UniformBufferObject ubo;
    
    ubo.model = mat4.identity();
    ubo.model = mat4.rotate(ubo.model, m_rotationAngle, m_rotationAxis);
    
    float[3] eye = [2.0f, 2.0f, 2.0f];
    float[3] center = [0.0f, 0.0f, 0.0f];
    float[3] up = [0.0f, 0.0f, 1.0f];
    ubo.view = mat4.lookAt(eye, center, up);
    
    int width, height;
    SDL_GetWindowSize(m_window, &width, &height);

    float aspect = cast(float) width / cast(float) height;
    ubo.proj = mat4.perspective(45.0f, aspect, 0.1f, 10.0f);
    
    return ubo;
  }

  void drawFrame() {
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(m_device);
    scope(exit) SDL_SubmitGPUCommandBuffer(cmdbuf);

    SDL_GPUTexture* swapchainTexture;
    SDL_WaitAndAcquireGPUSwapchainTexture(
      cmdbuf,
      m_window,
      &swapchainTexture,
      null,
      null
    );

    if (swapchainTexture !is null) {
      SDL_GPUColorTargetInfo colorTargetInfo;
      colorTargetInfo.texture = swapchainTexture;
      colorTargetInfo.clear_color = SDL_FColor(1.0f, 1.0f, 1.0f, 1.0f);
      colorTargetInfo.load_op = SDL_GPULoadOp.SDL_GPU_LOADOP_CLEAR;
      colorTargetInfo.store_op = SDL_GPUStoreOp.SDL_GPU_STOREOP_STORE;

      SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
        cmdbuf,
        &colorTargetInfo,
        1,
        null
      );
      scope(exit) SDL_EndGPURenderPass(renderPass);

      auto uniformMatrices = computeUniformMatrices();
      SDL_PushGPUVertexUniformData(
        cmdbuf,
        0,
        &uniformMatrices,
        uniformMatrices.sizeof
      );

      auto vertexBinding = SDL_GPUBufferBinding(m_vertexBuffer, 0);
      SDL_BindGPUVertexBuffers(
        renderPass,
        0,
        &vertexBinding,
        1
      );

      auto indexBinding = SDL_GPUBufferBinding(m_indexBuffer, 0);
      SDL_BindGPUIndexBuffer(
        renderPass,
        &indexBinding,
        SDL_GPU_INDEXELEMENTSIZE_16BIT
      );
      SDL_BindGPUGraphicsPipeline(
        renderPass,
        m_pipeline
      );

		  SDL_DrawGPUIndexedPrimitives(
        renderPass,
        s_indices.length,
        1,
        0,
        0,
        0
      );
    }
  }
}