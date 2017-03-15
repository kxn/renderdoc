/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <QDebug>
#include <QMap>
#include <QString>
#include <QVector>
#include "renderdoc_replay.h"

struct BoundResource
{
  BoundResource()
  {
    Id = ResourceId();
    HighestMip = -1;
    FirstSlice = -1;
    typeHint = CompType::Typeless;
  }
  BoundResource(ResourceId id)
  {
    Id = id;
    HighestMip = -1;
    FirstSlice = -1;
    typeHint = CompType::Typeless;
  }

  ResourceId Id;
  int HighestMip;
  int FirstSlice;
  CompType typeHint;
};

struct BoundVBuffer
{
  ResourceId Buffer;
  uint64_t ByteOffset;
  uint32_t ByteStride;
};

struct VertexInputAttribute
{
  QString Name;
  int VertexBuffer;
  uint32_t RelativeByteOffset;
  bool PerInstance;
  int InstanceRate;
  ResourceFormat Format;
  PixelValue GenericValue;
  bool Used;
};

struct Viewport
{
  float x, y, width, height;
};

class CommonPipelineState
{
public:
  CommonPipelineState() {}
  void SetStates(APIProperties props, D3D11Pipe::State *d3d11, D3D12PipelineState *d3d12,
                 GLPipe::State *gl, VKPipe::State *vk)
  {
    m_APIProps = props;
    m_D3D11 = d3d11;
    m_D3D12 = d3d12;
    m_GL = gl;
    m_Vulkan = vk;
  }

  GraphicsAPI DefaultType = GraphicsAPI::D3D11;

  bool LogLoaded()
  {
    return m_D3D11 != NULL || m_D3D12 != NULL || m_GL != NULL || m_Vulkan != NULL;
  }

  bool IsLogD3D11()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::D3D11 && m_D3D11 != NULL;
  }

  bool IsLogD3D12()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::D3D12 && m_D3D12 != NULL;
  }

  bool IsLogGL()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::OpenGL && m_GL != NULL;
  }

  bool IsLogVK()
  {
    return LogLoaded() && m_APIProps.pipelineType == GraphicsAPI::Vulkan && m_Vulkan != NULL;
  }

  // add a bunch of generic properties that people can check to save having to see which pipeline
  // state
  // is valid and look at the appropriate part of it
  bool IsTessellationEnabled()
  {
    if(LogLoaded())
    {
      if(IsLogD3D11())
        return m_D3D11 != NULL && m_D3D11->m_HS.Object != ResourceId();

      if(IsLogD3D12())
        return m_D3D12 != NULL && m_D3D12->m_HS.Object != ResourceId();

      if(IsLogGL())
        return m_GL != NULL && m_GL->m_TES.Object != ResourceId();

      if(IsLogVK())
        return m_Vulkan != NULL && m_Vulkan->m_TES.Object != ResourceId();
    }

    return false;
  }

  bool SupportsResourceArrays() { return LogLoaded() && IsLogVK(); }
  bool SupportsBarriers() { return LogLoaded() && (IsLogVK() || IsLogD3D12()); }
  // whether or not the PostVS data is aligned in the typical fashion
  // ie. vectors not crossing float4 boundaries). APIs that use stream-out
  // or transform feedback have tightly packed data, but APIs that rewrite
  // shaders to dump data might have these alignment requirements
  bool HasAlignedPostVSData() { return LogLoaded() && IsLogVK(); }
  QString GetImageLayout(ResourceId id);
  QString Abbrev(ShaderStage stage);
  QString OutputAbbrev();

  Viewport GetViewport(int index);
  const ShaderBindpointMapping &GetBindpointMapping(ShaderStage stage);
  const ShaderReflection *GetShaderReflection(ShaderStage stage);
  QString GetShaderEntryPoint(ShaderStage stage);
  ResourceId GetShader(ShaderStage stage);
  QString GetShaderName(ShaderStage stage);
  void GetIBuffer(ResourceId &buf, uint64_t &ByteOffset);
  bool IsStripRestartEnabled();
  uint32_t GetStripRestartIndex(uint32_t indexByteWidth);
  QVector<BoundVBuffer> GetVBuffers();
  QVector<VertexInputAttribute> GetVertexInputs();
  void GetConstantBuffer(ShaderStage stage, uint32_t BufIdx, uint32_t ArrayIdx, ResourceId &buf,
                         uint64_t &ByteOffset, uint64_t &ByteSize);
  QMap<BindpointMap, QVector<BoundResource>> GetReadOnlyResources(ShaderStage stage);
  QMap<BindpointMap, QVector<BoundResource>> GetReadWriteResources(ShaderStage stage);
  BoundResource GetDepthTarget();
  QVector<BoundResource> GetOutputTargets();

private:
  D3D11Pipe::State *m_D3D11 = NULL;
  D3D12PipelineState *m_D3D12 = NULL;
  GLPipe::State *m_GL = NULL;
  VKPipe::State *m_Vulkan = NULL;
  APIProperties m_APIProps;

  const D3D11Pipe::Shader &GetD3D11Stage(ShaderStage stage)
  {
    if(stage == ShaderStage::Vertex)
      return m_D3D11->m_VS;
    if(stage == ShaderStage::Domain)
      return m_D3D11->m_DS;
    if(stage == ShaderStage::Hull)
      return m_D3D11->m_HS;
    if(stage == ShaderStage::Geometry)
      return m_D3D11->m_GS;
    if(stage == ShaderStage::Pixel)
      return m_D3D11->m_PS;
    if(stage == ShaderStage::Compute)
      return m_D3D11->m_CS;

    qCritical() << "Error - invalid stage " << (int)stage;
    return m_D3D11->m_CS;
  }

  const D3D12PipelineState::Shader &GetD3D12Stage(ShaderStage stage)
  {
    if(stage == ShaderStage::Vertex)
      return m_D3D12->m_VS;
    if(stage == ShaderStage::Domain)
      return m_D3D12->m_DS;
    if(stage == ShaderStage::Hull)
      return m_D3D12->m_HS;
    if(stage == ShaderStage::Geometry)
      return m_D3D12->m_GS;
    if(stage == ShaderStage::Pixel)
      return m_D3D12->m_PS;
    if(stage == ShaderStage::Compute)
      return m_D3D12->m_CS;

    qCritical() << "Error - invalid stage " << (int)stage;
    return m_D3D12->m_CS;
  }

  const GLPipe::Shader &GetGLStage(ShaderStage stage)
  {
    if(stage == ShaderStage::Vertex)
      return m_GL->m_VS;
    if(stage == ShaderStage::Tess_Control)
      return m_GL->m_TCS;
    if(stage == ShaderStage::Tess_Eval)
      return m_GL->m_TES;
    if(stage == ShaderStage::Geometry)
      return m_GL->m_GS;
    if(stage == ShaderStage::Fragment)
      return m_GL->m_FS;
    if(stage == ShaderStage::Compute)
      return m_GL->m_CS;

    qCritical() << "Error - invalid stage " << (int)stage;
    return m_GL->m_CS;
  }

  const VKPipe::Shader &GetVulkanStage(ShaderStage stage)
  {
    if(stage == ShaderStage::Vertex)
      return m_Vulkan->m_VS;
    if(stage == ShaderStage::Tess_Control)
      return m_Vulkan->m_TCS;
    if(stage == ShaderStage::Tess_Eval)
      return m_Vulkan->m_TES;
    if(stage == ShaderStage::Geometry)
      return m_Vulkan->m_GS;
    if(stage == ShaderStage::Fragment)
      return m_Vulkan->m_FS;
    if(stage == ShaderStage::Compute)
      return m_Vulkan->m_CS;

    qCritical() << "Error - invalid stage " << (int)stage;
    return m_Vulkan->m_CS;
  }

public:
  QString GetShaderExtension();
};