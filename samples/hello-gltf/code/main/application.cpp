//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

///
/// Sample app demonstrating the loading of a .gltf file (hello world)
///

#include "application.hpp"
#include "main/applicationEntrypoint.hpp"
#include "gui/imguiVulkan.hpp"
#include "material/drawable.hpp"
#include "material/shaderManager.hpp"
#include "material/materialManager.hpp"
#include "camera/cameraController.hpp"
#include "camera/cameraControllerTouch.hpp"
#include "system/math_common.hpp"
#include "imgui.h"

#include <random>
#include <iostream>
#include <filesystem>

namespace
{
    static constexpr std::array<const char*, NUM_RENDER_PASSES> sRenderPassNames = { "RP_SCENE", "RP_HUD", "RP_BLIT" };

    glm::vec3 gCameraStartPos = glm::vec3(26.48f, 20.0f, -5.21f);
    glm::vec3 gCameraStartRot = glm::vec3(0.0f, 110.0f, 0.0f);

    float   gFOV = PI_DIV_4;
    float   gNearPlane = 1.0f;
    float   gFarPlane = 1800.0f;
    float   gNormalAmount = 0.3f;
    float   gNormalMirrorReflectAmount = 0.05f;

    const char* gMuseumAssetsPath = "Media\\Meshes";
    const char* gTextureFolder    = "Media\\Textures\\";
}

///
/// @brief Implementation of the Application entrypoint (called by the framework)
/// @return Pointer to Application (derived from @FrameworkApplicationBase).
/// Creates the Application class.  Ownership is passed to the calling (framework) function.
/// 
FrameworkApplicationBase* Application_ConstructApplication()
{
    return new Application();
}

Application::Application() : ApplicationHelperBase()
{
}

Application::~Application()
{
}

//-----------------------------------------------------------------------------
bool Application::Initialize(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    if (!ApplicationHelperBase::Initialize(windowHandle))
    {
        return false;
    }

    if (!InitializeLights())
    {
        return false;
    }

    if (!InitializeCamera())
    {
        return false;
    }

    if (!LoadShaders())
    {
        return false;
    }

    if (!InitUniforms())
    {
        return false;
    }

    if (!CreateRenderTargets())
    {
        return false;
    }

    if (!InitAllRenderPasses())
    {
        return false;
    }

    if (!InitGui(windowHandle))
    {
        return false;
    }

    if (!LoadMeshObjects())
    {
        return false;
    }

    if (!InitCommandBuffers())
    {
        return false;
    }

    if (!InitLocalSemaphores())
    {
        return false;
    }

    if (!BuildCmdBuffers())
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Destroy()
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    // Uniform Buffers
    ReleaseUniformBuffer(pVulkan, &m_ObjectVertUniform);
    ReleaseUniformBuffer(pVulkan, &m_LightUniform);
     
    for (auto& [hash, objectUniform] : m_ObjectFragUniforms)
    {
        ReleaseUniformBuffer(pVulkan, &objectUniform.objectFragUniform);
    }

    // Cmd buffers
    for (int whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        for (auto& cmdBuffer : m_RenderPassData[whichPass].PassCmdBuffer)
        {
            cmdBuffer.Release();
        }

        for (auto& cmdBuffer : m_RenderPassData[whichPass].ObjectsCmdBuffer)
        {
            cmdBuffer.Release();
        }

        m_RenderPassData[whichPass].RenderTarget.Release();
    }

    // Render passes / Semaphores
    for (int whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        vkDestroyRenderPass(m_vulkan->m_VulkanDevice, m_RenderPassData[whichPass].RenderPass, nullptr);
        vkDestroySemaphore(m_vulkan->m_VulkanDevice, m_RenderPassData[whichPass].PassCompleteSemaphore, nullptr);
    }

    // Drawables
    m_SceneDrawables.clear();
    m_BlitQuadDrawable.reset();

    // Textures
    for (auto& [key, texture] : m_LoadedTextures)
    {
        texture.Release(m_vulkan.get());
    }
    m_LoadedTextures.clear();

    // Internal
    m_ShaderManager.reset();
    m_MaterialManager.reset();
    m_CameraController.reset();
    m_AssetManager.reset();

    ApplicationHelperBase::Destroy();
}

//-----------------------------------------------------------------------------
bool Application::InitializeLights()
//-----------------------------------------------------------------------------
{
    m_LightUniformData.SpotLights_pos[0] = glm::vec4(-6.900000f, 32.299999f, -1.900000f, 1.0f);
    m_LightUniformData.SpotLights_pos[1] = glm::vec4(3.300000f, 26.900000f, 7.600000f, 1.0f);
    m_LightUniformData.SpotLights_pos[2] = glm::vec4(12.100000f, 41.400002f, -2.800000f, 1.0f);
    m_LightUniformData.SpotLights_pos[3] = glm::vec4(-5.400000f, 18.500000f, 28.500000f, 1.0f);

    m_LightUniformData.SpotLights_dir[0] = glm::vec4(-0.534696f, -0.834525f, 0.132924f, 0.0f);
    m_LightUniformData.SpotLights_dir[1] = glm::vec4(0.000692f, -0.197335f, 0.980336f, 0.0f);
    m_LightUniformData.SpotLights_dir[2] = glm::vec4(0.985090f, -0.172016f, 0.003000f, 0.0f);
    m_LightUniformData.SpotLights_dir[3] = glm::vec4(0.674125f, -0.295055f, -0.677125f, 0.0f);

    m_LightUniformData.SpotLights_color[0] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 3.000000f);
    m_LightUniformData.SpotLights_color[1] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 3.500000f);
    m_LightUniformData.SpotLights_color[2] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 2.000000f);
    m_LightUniformData.SpotLights_color[3] = glm::vec4(1.000000f, 1.000000f, 1.000000f, 2.800000f);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitializeCamera()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Initializing Camera...");
    LOGI("******************************");

    m_Camera.SetPosition(gCameraStartPos, glm::quat(gCameraStartRot * TO_RADIANS));
    m_Camera.SetAspect(float(gRenderWidth) / float(gRenderHeight));
    m_Camera.SetFov(gFOV);
    m_Camera.SetClipPlanes(gNearPlane, gFarPlane);

    // Camera Controller //

#if defined(OS_ANDROID)
    typedef CameraControllerTouch           tCameraController;
#else
    typedef CameraController                tCameraController;
#endif

    auto cameraController = std::make_unique<tCameraController>();
    if (!cameraController->Initialize(gRenderWidth, gRenderHeight))
    {
        return false;
    }

    m_CameraController = std::move(cameraController);

    return true;
}

//-----------------------------------------------------------------------------
bool Application::LoadShaders()
//-----------------------------------------------------------------------------
{
    m_ShaderManager = std::make_unique<ShaderManager>(*m_vulkan);
    m_ShaderManager->RegisterRenderPassNames(sRenderPassNames);

    m_MaterialManager = std::make_unique<MaterialManager>();

    LOGI("******************************");
    LOGI("Loading Shaders...");
    LOGI("******************************");

    typedef std::pair<std::string, std::string> tIdAndFilename;
    for (const tIdAndFilename& i :
            { tIdAndFilename { "Blit",  "Media\\Shaders\\Blit.json" },
              tIdAndFilename { "SceneOpaque", "Media\\Shaders\\SceneOpaque.json" },
              tIdAndFilename { "SceneTransparent", "Media\\Shaders\\SceneTransparent.json" }
            })
    {
        if (!m_ShaderManager->AddShader(*m_AssetManager, i.first, i.second))
        {
            LOGE("Error Loading shader %s from %s", i.first.c_str(), i.second.c_str());
            LOGI("Please verify if you have all required assets on the sample media folder");
            LOGI("If you are running on Android, don't forget to run the `02_CopyMediaToDevice.bat` script to copy all media files into the device memory");
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::CreateRenderTargets()
//-----------------------------------------------------------------------------
{
    LOGI("**************************");
    LOGI("Creating Render Targets...");
    LOGI("**************************");

    VkFormat desiredDepthFormat = m_vulkan->GetBestVulkanDepthFormat();

    const VkFormat MainColorType[] = { VK_FORMAT_R8G8B8A8_UNORM };
    const VkFormat HudColorType[]  = { VK_FORMAT_R8G8B8A8_UNORM };

    if (!m_RenderPassData[RP_SCENE].RenderTarget.Initialize(m_vulkan.get(), gRenderWidth, gRenderHeight, MainColorType, desiredDepthFormat, VK_SAMPLE_COUNT_1_BIT, "Scene RT"))
    {
        LOGE("Unable to create scene render target");
        return false;
    }

    // Notice no depth on the HUD RT
    if (!m_RenderPassData[RP_HUD].RenderTarget.Initialize(m_vulkan.get(), gSurfaceWidth, gSurfaceHeight, HudColorType, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT, "HUD RT"))
    {
        LOGE("Unable to create hud render target");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitUniforms()
//-----------------------------------------------------------------------------
{
    LOGI("******************************");
    LOGI("Initializing Uniforms...");
    LOGI("******************************");

    Vulkan* pVulkan = m_vulkan.get();

    if (!CreateUniformBuffer(pVulkan, m_ObjectVertUniform))
    {
        return false;
    }

    if (!CreateUniformBuffer(pVulkan, m_LightUniform))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitAllRenderPasses()
//-----------------------------------------------------------------------------
{
    //                                       ColorInputUsage |               ClearDepthRenderPass | ColorOutputUsage |                     DepthOutputUsage |              ClearColor
    m_RenderPassData[RP_SCENE].PassSetup = { RenderPassInputUsage::Clear,    true,                  RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Store,   {}};
    m_RenderPassData[RP_HUD].PassSetup   = { RenderPassInputUsage::Clear,    false,                 RenderPassOutputUsage::StoreReadOnly,  RenderPassOutputUsage::Discard, {}};
    m_RenderPassData[RP_BLIT].PassSetup  = { RenderPassInputUsage::DontCare, true,                  RenderPassOutputUsage::Present,        RenderPassOutputUsage::Discard, {}};

    auto swapChainColorFormat = tcb::span<const VkFormat>({ &m_vulkan->m_SurfaceFormat, 1 });
    auto swapChainDepthFormat = m_vulkan->m_SwapchainDepth.format;

    LOGI("******************************");
    LOGI("Initializing Render Passes... ");
    LOGI("******************************");

    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        bool isSwapChainRenderPass = whichPass == RP_BLIT;

        tcb::span<const VkFormat> colorFormats = isSwapChainRenderPass ? swapChainColorFormat : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
        VkFormat                  depthFormat  = isSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

        const auto& passSetup = m_RenderPassData[whichPass].PassSetup;
        
        if (!m_vulkan->CreateRenderPass(
            { colorFormats },
            depthFormat,
            VK_SAMPLE_COUNT_1_BIT,
            passSetup.ColorInputUsage,
            passSetup.ColorOutputUsage,
            passSetup.ClearDepthRenderPass,
            passSetup.DepthOutputUsage,
            & m_RenderPassData[whichPass].RenderPass))
        {
            return false;
        }
            
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitGui(uintptr_t windowHandle)
//-----------------------------------------------------------------------------
{
    const auto& hudRenderTarget = m_RenderPassData[RP_HUD].RenderTarget;
    m_Gui = std::make_unique<GuiImguiVulkan>(*m_vulkan, m_RenderPassData[RP_HUD].RenderPass);
    if (!m_Gui->Initialize(windowHandle, hudRenderTarget[0].m_Width, hudRenderTarget[0].m_Height))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
VulkanTexInfo* Application::GetOrLoadTexture(const char* textureName)
//-----------------------------------------------------------------------------
{
    if (textureName == nullptr || textureName[0] == 0)
    {
        return nullptr;
    }

    auto texturePath = std::filesystem::path(textureName);
    if (!texturePath.has_filename())
    {
        return nullptr;
    }

    auto textureFilename = texturePath.stem();

    auto iter = m_LoadedTextures.find(textureFilename.string());
    if (iter != m_LoadedTextures.end())
    {
        return &iter->second;
    }

    // Prepare the texture path
    std::string textureInternalPath = gTextureFolder;
    textureInternalPath.append(textureFilename.string());
    textureInternalPath.append(".ktx");

    auto loadedTexture = LoadKTXTexture(m_vulkan.get(), *m_AssetManager, textureInternalPath.c_str());
    if (!loadedTexture.IsEmpty())
    {
        m_LoadedTextures.insert({ textureFilename.string() , std::move(loadedTexture) });
        VulkanTexInfo* texInfo = &m_LoadedTextures[textureFilename.string()];
        return texInfo;
    }

    return nullptr;
}

//-----------------------------------------------------------------------------
bool Application::LoadMeshObjects()
//-----------------------------------------------------------------------------
{
    LOGI("***********************");
    LOGI("Initializing Meshes... ");
    LOGI("***********************");

    const auto* pSceneOpaqueShader      = m_ShaderManager->GetShader("SceneOpaque");
    const auto* pSceneTransparentShader = m_ShaderManager->GetShader("SceneTransparent");
    const auto* pBlitQuadShader = m_ShaderManager->GetShader("Blit");
    if (!pSceneOpaqueShader || !pSceneTransparentShader || !pBlitQuadShader)
    {
        return false;
    }
    
    LOGI("***********************************");
    LOGI("Loading and preparing the museum...");
    LOGI("***********************************");

    auto* whiteTexture         = GetOrLoadTexture("white_d.ktx");
    auto* blackTexture         = GetOrLoadTexture("black_d.ktx");
    auto* normalDefaultTexture = GetOrLoadTexture("normal_default.ktx");

    if (!whiteTexture || !blackTexture || !normalDefaultTexture)
    {
        LOGE("Failed to load supporting textures");
        return false;
    }

    auto UniformBufferLoader = [&](const ObjectMaterialParameters& objectMaterialParameters) -> ObjectMaterialParameters&
    {
        auto hash = objectMaterialParameters.GetHash();

        auto iter = m_ObjectFragUniforms.try_emplace(hash, ObjectMaterialParameters());
        if (iter.second)
        {
            Vulkan* pVulkan = m_vulkan.get();
            iter.first->second.objectFragUniformData = objectMaterialParameters.objectFragUniformData;
            if (!CreateUniformBuffer(pVulkan, iter.first->second.objectFragUniform))
            {
                LOGE("Failed to create object uniform buffer");
            }
        }

        return iter.first->second;
    };

    auto MaterialLoader = [&](const MeshObjectIntermediate::MaterialDef& materialDef)->std::optional<Material>
    {
        auto* diffuseTexture           = GetOrLoadTexture(materialDef.diffuseFilename.c_str());
        auto* normalTexture            = GetOrLoadTexture(materialDef.bumpFilename.c_str());
        auto* emissiveTexture          = GetOrLoadTexture(materialDef.emissiveFilename.c_str());
        auto* metallicRoughnessTexture = GetOrLoadTexture(materialDef.specMapFilename.c_str());
        bool alphaCutout               = materialDef.alphaCutout;
        bool transparent               = materialDef.transparent;

        const Shader* targetShader = transparent ? pSceneTransparentShader : pSceneOpaqueShader;

        ObjectMaterialParameters objectMaterial;
        objectMaterial.objectFragUniformData.Color.r = static_cast<float>(materialDef.baseColorFactor[0]);
        objectMaterial.objectFragUniformData.Color.g = static_cast<float>(materialDef.baseColorFactor[1]);
        objectMaterial.objectFragUniformData.Color.b = static_cast<float>(materialDef.baseColorFactor[2]);
        objectMaterial.objectFragUniformData.Color.a = static_cast<float>(materialDef.baseColorFactor[3]);
        objectMaterial.objectFragUniformData.ORM.b   = static_cast<float>(materialDef.metallicFactor);
        objectMaterial.objectFragUniformData.ORM.g   = static_cast<float>(materialDef.roughnessFactor);

        auto shaderMaterial = m_MaterialManager->CreateMaterial(*m_vulkan.get(), *targetShader, NUM_VULKAN_BUFFERS,
            [&](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
            {
                if (texName == "Diffuse")
                {
                    return { diffuseTexture ? diffuseTexture : whiteTexture };
                }
                if (texName == "Normal")
                {
                    return { normalTexture ? normalTexture : normalDefaultTexture };
                }
                if (texName == "Emissive")
                {
                    return { emissiveTexture ? emissiveTexture : blackTexture };
                }
                if (texName == "MetallicRoughness")
                {
                    return { metallicRoughnessTexture ? metallicRoughnessTexture : blackTexture };
                }

                return {};
            },
            [&](const std::string& bufferName) -> MaterialPass::tPerFrameVkBuffer
            {
                if (bufferName == "Vert")
                {
                    return { m_ObjectVertUniform.buf.GetVkBuffer() };
                }
                else if (bufferName == "Frag")
                {
                    return { UniformBufferLoader(objectMaterial).objectFragUniform.buf.GetVkBuffer()};
                }
                else if (bufferName == "Light")
                {
                    return { m_LightUniform.buf.GetVkBuffer() };
                }

                return {};
            }
            );

        return shaderMaterial;
    };

    std::vector<std::string> meshFiles;
    
    if (std::filesystem::is_directory(gMuseumAssetsPath))
    {
        for (auto& p : std::filesystem::directory_iterator(gMuseumAssetsPath))
        {
            if (p.path().extension() == ".gltf")
            {
                meshFiles.push_back(p.path().string());
            }
        }
    }

#if defined(OS_ANDROID)
    // std::filesystem above will not work on Android, we need to specify the file directly
    meshFiles.push_back("Media\\Meshes\\Museum.gltf");
#endif

    if (meshFiles.size() == 0)
    {
        LOGE("No meshes found to render!");
    }
    else
    {
        LOGI("Found %d meshes", static_cast<int>(meshFiles.size()));
    }

    for (auto& meshFile : meshFiles)
    {
        bool sceneMeshResult = DrawableLoader::LoadDrawables(
            *m_vulkan,
            *m_AssetManager,
            { &m_RenderPassData[RP_SCENE].RenderPass, 1 },
            &sRenderPassNames[RP_SCENE],
            meshFile,
            MaterialLoader,
            m_SceneDrawables,
            {},    // RenderPassMultisample 
            false, // UseInstancing
            {});   // RenderPassSubpasses
        if (!sceneMeshResult)
        {
            LOGE("Error Loading the %s gltf file", meshFile.c_str());
            LOGI("Please verify if you have all required assets on the sample media folder");
            LOGI("If you are running on Android, don't forget to run the `02_CopyMediaToDevice.bat` script to copy all media files into the device memory");
            return false;
        }
    }

    LOGI("*********************");
    LOGI("Creating Quad mesh...");
    LOGI("*********************");

    MeshObject blitQuadMesh; //                       PosLLRadius |                        UVLLRadius
    MeshObject::CreateScreenSpaceMesh(m_vulkan.get(), glm::vec4(-1.0f, -1.0f, 2.0f, 2.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), 0, &blitQuadMesh);

    // Blit Material
    auto blitQuadShaderMaterial = m_MaterialManager->CreateMaterial(*m_vulkan.get(), *pBlitQuadShader, m_vulkan->m_SwapchainImageCount,
        [this](const std::string& texName) -> const MaterialPass::tPerFrameTexInfo
        {
            if (texName == "Diffuse")
            {
                return { &m_RenderPassData[RP_SCENE].RenderTarget[0].m_ColorAttachments[0] };
            }
            else if (texName == "Overlay")
            {
                return { &m_RenderPassData[RP_HUD].RenderTarget[0].m_ColorAttachments[0] };
            }
            return {};
        },
        [this](const std::string& bufferName) -> MaterialPass::tPerFrameVkBuffer
        {
            return {};
        }
        );

    m_BlitQuadDrawable = std::make_unique<Drawable>(*m_vulkan.get(), std::move(blitQuadShaderMaterial));
    if (!m_BlitQuadDrawable->Init(m_RenderPassData[RP_BLIT].RenderPass, sRenderPassNames[RP_BLIT], std::move(blitQuadMesh)))
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitCommandBuffers()
//-----------------------------------------------------------------------------
{
    LOGI("*******************************");
    LOGI("Initializing Command Buffers...");
    LOGI("*******************************");

    Vulkan* pVulkan = m_vulkan.get();

    auto GetPassName = [](uint32_t whichPass)
    {
        if (whichPass >= sRenderPassNames.size())
        {
            LOGE("GetPassName() called with unknown pass (%d)!", whichPass);
            return "RP_UNKNOWN";
        }

        return sRenderPassNames[whichPass];
    };

    m_RenderPassData[RP_SCENE].PassCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_HUD].PassCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_HUD].ObjectsCmdBuffer.resize(NUM_VULKAN_BUFFERS);
    m_RenderPassData[RP_BLIT].PassCmdBuffer.resize(m_vulkan->m_SwapchainImageCount);
    m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.resize(m_vulkan->m_SwapchainImageCount);

    char szName[256];
    const VkCommandBufferLevel CmdBuffLevel = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        for (uint32_t whichBuffer = 0; whichBuffer < m_RenderPassData[whichPass].PassCmdBuffer.size(); whichBuffer++)
        {
            // The Pass Command Buffer => Primary
            sprintf(szName, "Primary (%s; Buffer %d of %d)", GetPassName(whichPass), whichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Initialize(pVulkan, szName, VK_COMMAND_BUFFER_LEVEL_PRIMARY))
            {
                return false;
            }

            // Model => Secondary
            sprintf(szName, "Model (%s; Buffer %d of %d)", GetPassName(whichPass), whichBuffer + 1, NUM_VULKAN_BUFFERS);
            if (!m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].Initialize(pVulkan, szName, CmdBuffLevel))
            {
                return false;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::InitLocalSemaphores()
//-----------------------------------------------------------------------------
{
    LOGI("********************************");
    LOGI("Initializing Local Semaphores...");
    LOGI("********************************");

    const VkSemaphoreCreateInfo SemaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        VkResult retVal = vkCreateSemaphore(m_vulkan->m_VulkanDevice, &SemaphoreInfo, NULL, &m_RenderPassData[whichPass].PassCompleteSemaphore);
        if (!CheckVkError("vkCreateSemaphore()", retVal))
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
bool Application::BuildCmdBuffers()
//-----------------------------------------------------------------------------
{
    LOGI("***************************");
    LOGI("Building Command Buffers...");
    LOGI("****************************");

    // Begin recording
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        auto& renderPassData         = m_RenderPassData[whichPass];
        bool  bisSwapChainRenderPass = whichPass == RP_BLIT;

        for (uint32_t whichBuffer = 0; whichBuffer < renderPassData.ObjectsCmdBuffer.size(); whichBuffer++)
        {
            auto& cmdBufer = renderPassData.ObjectsCmdBuffer[whichBuffer];

            uint32_t targetWidth  = bisSwapChainRenderPass ? m_vulkan->m_SurfaceWidth : renderPassData.RenderTarget[0].m_Width;
            uint32_t targetHeight = bisSwapChainRenderPass ? m_vulkan->m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

            VkViewport viewport = {};
            viewport.x          = 0.0f;
            viewport.y          = 0.0f;
            viewport.width      = (float)targetWidth;
            viewport.height     = (float)targetHeight;
            viewport.minDepth   = 0.0f;
            viewport.maxDepth   = 1.0f;

            VkRect2D scissor      = {};
            scissor.offset.x      = 0;
            scissor.offset.y      = 0;
            scissor.extent.width  = targetWidth;
            scissor.extent.height = targetHeight;

            // Set up some values that change based on render pass
            VkRenderPass  whichRenderPass  = renderPassData.RenderPass;
            VkFramebuffer whichFramebuffer = bisSwapChainRenderPass ? m_vulkan->m_pSwapchainFrameBuffers[whichBuffer] : renderPassData.RenderTarget[0].m_FrameBuffer;

            // Objects (can render into any pass except Blit)
            if (!cmdBufer.Begin(whichFramebuffer, whichRenderPass, bisSwapChainRenderPass))
            {
                return false;
            }
            vkCmdSetViewport(cmdBufer.m_VkCommandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(cmdBufer.m_VkCommandBuffer, 0, 1, &scissor);
        }
    }
    
    // Scene drawables
    for (const auto& sceneDrawable : m_SceneDrawables)
    {
        AddDrawableToCmdBuffers(sceneDrawable, m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_SCENE].ObjectsCmdBuffer.size()));
    }

    // Blit quad drawable
    AddDrawableToCmdBuffers(*m_BlitQuadDrawable.get(), m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.data(), 1, static_cast<uint32_t>(m_RenderPassData[RP_BLIT].ObjectsCmdBuffer.size()));

    // End recording
    for (uint32_t whichPass = 0; whichPass < NUM_RENDER_PASSES; whichPass++)
    {
        auto& renderPassData = m_RenderPassData[whichPass];

        for (uint32_t whichBuffer = 0; whichBuffer < renderPassData.ObjectsCmdBuffer.size(); whichBuffer++)
        {
            auto& cmdBufer = renderPassData.ObjectsCmdBuffer[whichBuffer];
            if (!cmdBufer.End())
            {
                return false;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::UpdateGui()
//-----------------------------------------------------------------------------
{
    if (m_Gui)
    {
        m_Gui->Update();
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::Begin("FPS", (bool*)nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::Text("FPS: %.1f", m_CurrentFPS);
            ImGui::Text("Camera [%f, %f, %f]", m_Camera.Position().x, m_Camera.Position().y, m_Camera.Position().z);
            ImGui::DragFloat3("Sun Dir", &m_LightUniformData.LightDirection.x, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat3("Sun Color", &m_LightUniformData.LightColor.x, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Sun Intensity", &m_LightUniformData.LightColor.w, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat3("Ambient Color", &m_LightUniformData.AmbientColor.x, 0.01f, 0.0f, 1.0f);

            for (int i = 0; i < NUM_SPOT_LIGHTS; i++)
            {
                std::string childName = std::string("Spot Light ").append(std::to_string(i+1));
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", childName.c_str());

                if (ImGui::CollapsingHeader(childName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
                {
                    ImGui::PushID(i);

                    ImGui::DragFloat3("Pos", &m_LightUniformData.SpotLights_pos[i].x, 0.1f);
                    ImGui::DragFloat3("Dir", &m_LightUniformData.SpotLights_dir[i].x, 0.01f, -1.0f, 1.0f);
                    ImGui::DragFloat3("Color", &m_LightUniformData.SpotLights_color[i].x, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Intensity", &m_LightUniformData.SpotLights_color[i].w, 0.1f, 0.0f, 100.0f);

                    ImGui::PopID();
                }

                ImDrawList* list = ImGui::GetWindowDrawList();

                glm::vec3 LightDirNotNormalized = m_LightUniformData.SpotLights_dir[i];
                LightDirNotNormalized = glm::normalize(LightDirNotNormalized);
                m_LightUniformData.SpotLights_dir[i] = glm::vec4(LightDirNotNormalized, 0.0f);
            }

            glm::vec3 LightDirNotNormalized   = m_LightUniformData.LightDirection;
            LightDirNotNormalized             = glm::normalize(LightDirNotNormalized);
            m_LightUniformData.LightDirection = glm::vec4(LightDirNotNormalized, 0.0f);
        }
        ImGui::End();

        return;
    }
}

//-----------------------------------------------------------------------------
bool Application::UpdateUniforms(uint32_t whichBuffer)
//-----------------------------------------------------------------------------
{
    Vulkan* pVulkan = m_vulkan.get();

    // Vert data
    {
        glm::mat4 LocalModel = glm::mat4(1.0f);
        LocalModel           = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        LocalModel           = glm::scale(LocalModel, glm::vec3(1.0f));
        glm::mat4 LocalMVP   = m_Camera.ProjectionMatrix() * m_Camera.ViewMatrix() * LocalModel;

        m_ObjectVertUniformData.MVPMatrix   = LocalMVP;
        m_ObjectVertUniformData.ModelMatrix = LocalModel;
        UpdateUniformBuffer(pVulkan, m_ObjectVertUniform, m_ObjectVertUniformData);
    }

    // Frag data
    for (auto& [hash, objectUniform] : m_ObjectFragUniforms)
    {
        UpdateUniformBuffer(pVulkan, objectUniform.objectFragUniform, objectUniform.objectFragUniformData);
    }

    // Light data
    {
        glm::mat4 CameraViewInv       = glm::inverse(m_Camera.ViewMatrix());
        glm::mat4 CameraProjection    = m_Camera.ProjectionMatrix();
        glm::mat4 CameraProjectionInv = glm::inverse(CameraProjection);

        m_LightUniformData.ProjectionInv     = CameraProjectionInv;
        m_LightUniformData.ViewInv           = CameraViewInv;
        m_LightUniformData.ViewProjectionInv = glm::inverse(CameraProjection * m_Camera.ViewMatrix());
        m_LightUniformData.ProjectionInvW    = glm::vec4(CameraProjectionInv[0].w, CameraProjectionInv[1].w, CameraProjectionInv[2].w, CameraProjectionInv[3].w);
        m_LightUniformData.CameraPos         = glm::vec4(m_Camera.Position(), 0.0f);

        UpdateUniformBuffer(pVulkan, m_LightUniform, m_LightUniformData);
    }

    return true;
}

//-----------------------------------------------------------------------------
void Application::Render(float fltDiffTime)
//-----------------------------------------------------------------------------
{
    // Obtain the next swap chain image for the next frame.
    auto currentVulkanBuffer = m_vulkan->SetNextBackBuffer();
    uint32_t whichBuffer     = currentVulkanBuffer.idx;

    // ********************************
    // Application Draw() - Begin
    // ********************************

    UpdateGui();

    // Update camera
    m_Camera.UpdateController(fltDiffTime * 10.0f, *m_CameraController);
    m_Camera.UpdateMatrices();
 
    // Update uniform buffers with latest data
    UpdateUniforms(whichBuffer);

    // First time through, wait for the back buffer to be ready
    tcb::span<const VkSemaphore> pWaitSemaphores = { &currentVulkanBuffer.semaphore, 1 };

    const VkPipelineStageFlags DefaultGfxWaitDstStageMasks[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    // RP_SCENE
    {
        BeginRenderPass(whichBuffer, RP_SCENE, currentVulkanBuffer.swapchainPresentIdx);
        AddPassCommandBuffer(whichBuffer, RP_SCENE);
        EndRenderPass(whichBuffer, RP_SCENE);

        // Submit the commands to the queue.
        SubmitRenderPass(whichBuffer, RP_SCENE, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_RenderPassData[RP_SCENE].PassCompleteSemaphore,1 });
        pWaitSemaphores = { &m_RenderPassData[RP_SCENE].PassCompleteSemaphore, 1 };
    }

    // RP_HUD
    VkCommandBuffer guiCommandBuffer = VK_NULL_HANDLE;
    if (m_Gui)
    {
        // Render gui (has its own command buffer, optionally returns vk_null_handle if not rendering anything)
        guiCommandBuffer = m_Gui->Render(whichBuffer, m_RenderPassData[RP_HUD].RenderTarget[0].m_FrameBuffer);
        if (guiCommandBuffer != VK_NULL_HANDLE)
        {
            BeginRenderPass(whichBuffer, RP_HUD, currentVulkanBuffer.swapchainPresentIdx);
            vkCmdExecuteCommands(m_RenderPassData[RP_HUD].PassCmdBuffer[whichBuffer].m_VkCommandBuffer, 1, &guiCommandBuffer);
            EndRenderPass(whichBuffer, RP_HUD);

            // Submit the commands to the queue.
            SubmitRenderPass(whichBuffer, RP_HUD, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_RenderPassData[RP_HUD].PassCompleteSemaphore,1 });
            pWaitSemaphores = { &m_RenderPassData[RP_HUD].PassCompleteSemaphore,1 };
        }
    }

    // Blit Results to the screen
    {
        BeginRenderPass(whichBuffer, RP_BLIT, currentVulkanBuffer.swapchainPresentIdx);
        AddPassCommandBuffer(whichBuffer, RP_BLIT);
        EndRenderPass(whichBuffer, RP_BLIT);

        // Submit the commands to the queue.
        SubmitRenderPass(whichBuffer, RP_BLIT, pWaitSemaphores, DefaultGfxWaitDstStageMasks, { &m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1 }, currentVulkanBuffer.fence);
        pWaitSemaphores = { &m_RenderPassData[RP_BLIT].PassCompleteSemaphore,1 };
    }

    // Queue is loaded up, tell the driver to start processing
    m_vulkan->PresentQueue(pWaitSemaphores, currentVulkanBuffer.swapchainPresentIdx);

    // ********************************
    // Application Draw() - End
    // ********************************
}

//-----------------------------------------------------------------------------
void Application::BeginRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass, uint32_t WhichSwapchainImage)
//-----------------------------------------------------------------------------
{
    auto& renderPassData         = m_RenderPassData[whichPass];
    bool  bisSwapChainRenderPass = whichPass == RP_BLIT;

    if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Reset())
    {
        LOGE("Pass (%d) command buffer Reset() failed !", whichPass);
    }

    if (!m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].Begin())
    {
        LOGE("Pass (%d) command buffer Begin() failed !", whichPass);
    }

    VkFramebuffer framebuffer = nullptr;
    switch (whichPass)
    {
    case RP_SCENE:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_HUD:
        framebuffer = m_RenderPassData[whichPass].RenderTarget[0].m_FrameBuffer;
        break;
    case RP_BLIT:
        framebuffer = m_vulkan->m_pSwapchainFrameBuffers[WhichSwapchainImage];
        break;
    default:
        framebuffer = nullptr;
        break;
    }

    assert(framebuffer != nullptr);

    VkRect2D passArea = {};
    passArea.offset.x = 0;
    passArea.offset.y = 0;
    passArea.extent.width  = bisSwapChainRenderPass ? m_vulkan->m_SurfaceWidth  : renderPassData.RenderTarget[0].m_Width;
    passArea.extent.height = bisSwapChainRenderPass ? m_vulkan->m_SurfaceHeight : renderPassData.RenderTarget[0].m_Height;

    auto                      swapChainColorFormat = tcb::span<const VkFormat>({ &m_vulkan->m_SurfaceFormat, 1 });
    auto                      swapChainDepthFormat = m_vulkan->m_SwapchainDepth.format;
    tcb::span<const VkFormat> colorFormats         = bisSwapChainRenderPass ? swapChainColorFormat : m_RenderPassData[whichPass].RenderTarget[0].m_pLayerFormats;
    VkFormat                  depthFormat          = bisSwapChainRenderPass ? swapChainDepthFormat : m_RenderPassData[whichPass].RenderTarget[0].m_DepthFormat;

    VkClearColorValue clearColor = { renderPassData.PassSetup.ClearColor[0], renderPassData.PassSetup.ClearColor[1], renderPassData.PassSetup.ClearColor[2], renderPassData.PassSetup.ClearColor[3] };

    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].BeginRenderPass(
        passArea,
        0.0f,
        1.0f,
        { &clearColor , 1 },
        (uint32_t)colorFormats.size(),
        depthFormat != VK_FORMAT_UNDEFINED,
        m_RenderPassData[whichPass].RenderPass,
        bisSwapChainRenderPass,
        framebuffer,
        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}


//-----------------------------------------------------------------------------
void Application::AddPassCommandBuffer(uint32_t whichBuffer, RENDER_PASS whichPass)
//-----------------------------------------------------------------------------
{
    if (m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].m_NumDrawCalls)
    {
        vkCmdExecuteCommands(m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].m_VkCommandBuffer, 1, &m_RenderPassData[whichPass].ObjectsCmdBuffer[whichBuffer].m_VkCommandBuffer);
    }
}

//-----------------------------------------------------------------------------
void Application::EndRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass)
//-----------------------------------------------------------------------------
{
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].EndRenderPass();
}

//-----------------------------------------------------------------------------
void Application::SubmitRenderPass(uint32_t whichBuffer, RENDER_PASS whichPass, const tcb::span<const VkSemaphore> WaitSemaphores, const tcb::span<const VkPipelineStageFlags> WaitDstStageMasks, tcb::span<VkSemaphore> SignalSemaphores, VkFence CompletionFence)
//-----------------------------------------------------------------------------
{
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].End();
    m_RenderPassData[whichPass].PassCmdBuffer[whichBuffer].QueueSubmit(WaitSemaphores, WaitDstStageMasks, SignalSemaphores, CompletionFence);
}
