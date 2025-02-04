//==================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//===================================================================================================
#pragma once

///
/// @file application.hpp
/// @brief Application to test gl_SampleMaskIn shader intrinsic.
/// 
/// Very simple application that attempts to use gl_SampleMaskIn.
/// 

#include "main/applicationHelperBase.hpp"
#include <map>
#include <string>

// Forward declarations
class Drawable;
class ShaderManager;
class MaterialManager;
struct ImageInfo;


class Application : public ApplicationHelperBase
{
public:
    Application();
    ~Application() override;

    //
    // Override ApplicationHelperBase
    //

    /// Override surface format selection.
    int PreInitializeSelectSurfaceFormat(tcb::span<const VkSurfaceFormatKHR> formats) override;

    /// Override Application entry point!
    bool Initialize(uintptr_t windowHandle) override;

    bool LoadShaders();

    bool InitFramebuffersRenderPassesAndDrawables();
    bool LoadSceneDrawables( VkRenderPass renderPass, uint32_t subpassIdx, VkSampleCountFlagBits passMsaa );
    std::unique_ptr<Drawable> InitFullscreenDrawable( const char* pShaderName, const std::map<const std::string, const VulkanTexInfo*>& inputLookup, const std::map<const std::string, const ImageInfo>& ImageAttachmentsLookup, VkRenderPass renderPass, uint32_t subpassIdx, VkSampleCountFlagBits passMsaa );
    bool InitCommandBuffers();
    bool Create2SubpassRenderPass( const tcb::span<const VkFormat> InternalColorFormats, const tcb::span<const VkFormat> OutputColorFormats, VkFormat InternalDepthFormat, VkSampleCountFlagBits InternalMsaa, VkSampleCountFlagBits OutputMsaa, VkRenderPass* pRenderPass/*out*/ );
    bool InitHdr();

    /// @brief Ticked every frame (by the Framework)
    /// @param fltDiffTime time (in seconds) since the last call to Render.
    void Render( float fltDiffTime ) override;

    void UpdateCamera( float elapsedTime );
    bool UpdateUniforms( uint32_t bufferIdx );
    bool UpdateCommandBuffer( uint32_t bufferIdx );

    void ChangeMsaaMode();

    bool InitGui( uintptr_t windowHandle );
    void UpdateGui();

    /// Shutdown function
    void Destroy() override;

private:
    // Textures
    std::map<std::string, VulkanTexInfo>    m_LoadedTextures;

    VulkanTexInfo m_TexWhite;

    // Shaders
    std::unique_ptr<ShaderManager>          m_ShaderManager;

    // Materials
    std::unique_ptr<MaterialManager>        m_MaterialManager;

    // Drawable(s) for rendering the main scene object
    std::vector<Drawable>                   m_SceneObject;

    // Drawable for tonemapping
    std::unique_ptr<Drawable>               m_TonemapDrawable;

    // Drawable for final blit to back buffer
    std::unique_ptr<Drawable>               m_BlitDrawable;

    // Uniform buffers
    struct ObjectVertUniform
    {
        glm::mat4 MVPMatrix;
        glm::mat4 ModelMatrix;
    };
    UniformT<ObjectVertUniform>             m_ObjectVertUniform;
    struct ObjectfragUniform
    {
        float tmp;
    };
    UniformT<ObjectfragUniform>             m_ObjectFragUniform;

    // Render target for Objects.
    CRenderTargetArray<1>                   m_LinearColorRT;

    // Render target for tonemap (when not running as part of a subpass chain)
    CRenderTargetArray<1>                   m_TonemapRT;

    // Render target for GUI.
    CRenderTargetArray<1>                   m_GuiRT;

    // Single command buffer
    Wrap_VkCommandBuffer                    m_CommandBuffer[NUM_VULKAN_BUFFERS];

    uint32_t                                m_TonemapSubPassIdx = 0;
    VkRenderPass                            m_BlitRenderPass = VK_NULL_HANDLE;

    bool                                    m_RequestedShaderResolve = false;
    bool                                    m_RequestedUseSubpasses = false;
    bool                                    m_RequestedUseRenderPassTransform = false;
    VkSampleCountFlagBits                   m_RequestedMsaa = VK_SAMPLE_COUNT_1_BIT;
};
