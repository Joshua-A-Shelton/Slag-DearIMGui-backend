#ifndef IMGUI_IMPL_SLAG_H
#define IMGUI_IMPL_SLAG_H

#include <slag/SlagLib.h>
#include <imgui.h>
#include <array>

struct ImGui_ImplSlag_Data
{
    slag::ShaderPipeline* shaderPipeline = nullptr;
    slag::Sampler* sampler = nullptr;
    slag::DescriptorPool* descriptorPool = nullptr;
    slag::PlatformData platformData;
    slag::Pixels::Format backBufferFormat = slag::Pixels::UNDEFINED;
    //dear imgui managed resources
    slag::Texture* fontsTexture = nullptr;
    slag::DescriptorBundle* fontsTextureBundle=nullptr;
};

struct ImGui_ImplSlag_ViewportData
{
    slag::Swapchain* swapchain= nullptr;
    bool outsideManaged = false;
    std::array<slag::Buffer*,3> drawDataArrays={nullptr, nullptr, nullptr};
    std::array<slag::Buffer*,3> drawDataIndexArrays{nullptr, nullptr, nullptr};
    ~ImGui_ImplSlag_ViewportData()
    {
        if(swapchain && !outsideManaged)
        {
            delete swapchain;
        }
        for(auto i=0; i< drawDataArrays.size(); i++)
        {
            if(drawDataArrays[i])
            {
                delete drawDataArrays[i];
            }
            if(drawDataIndexArrays[i])
            {
                delete drawDataIndexArrays[i];
            }
        }
    }
};

struct ImGui_ImplSlag_RenderState
{
    slag::CommandBuffer* commandBuffer = nullptr;
    slag::ShaderPipeline* shader = nullptr;
};

// public facing functions
IMGUI_IMPL_API bool     ImGui_ImplSlag_Init(slag::Swapchain* mainSwapchain, slag::PlatformData platformData, slag::ShaderPipeline* shaderPipeline, slag::Sampler* sampler, slag::Pixels::Format backBufferFormat);
IMGUI_IMPL_API void     ImGui_ImplSlag_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplSlag_NewFrame(slag::DescriptorPool* framePool);
IMGUI_IMPL_API void     ImGui_ImplSlag_RenderDrawData(ImDrawData* draw_data,slag::CommandBuffer* commandBuffer);

#endif //IMGUI_IMPL_SLAG_H
