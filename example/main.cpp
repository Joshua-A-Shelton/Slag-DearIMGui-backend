#include <iostream>
#define SDL_MAIN_HANDLED
#ifdef __linux
#define SLAG_X11_BACKEND
#endif
#include <SDL.h>
#include <slag/SlagLib.h>
#include <SDL_syswm.h>
#include "imgui_impl_sdl2.h"
#include "../imgui_impl_slag.h"
#include <glm/glm.hpp>

class DefaultFrameResources: public slag::FrameResources
{
public:
    slag::CommandBuffer* commandBuffer = nullptr;
    slag::DescriptorPool* descriptorPool = nullptr;
    DefaultFrameResources()
    {
        commandBuffer = slag::CommandBuffer::newCommandBuffer(slag::GpuQueue::GRAPHICS);
        descriptorPool = slag::DescriptorPool::newDescriptorPool();
    }
    ~DefaultFrameResources()override
    {
        if(commandBuffer)
        {
            delete commandBuffer;
        }
        if(descriptorPool)
        {
            delete descriptorPool;
        }
    }
    void waitForResourcesToFinish()override
    {
        commandBuffer->waitUntilFinished();
    }
    bool isFinished()override
    {
        return commandBuffer->isFinished();
    }
};

slag::FrameResources* generateDefaultFrameResources(size_t frameIndex, slag::Swapchain* chain)
{
    return new DefaultFrameResources();
}
//this is only an example, it may change depending on with windowing backend
void* extractNativeWindowHandle(ImGuiViewport* viewport)
{
#ifdef _WIN32
    return viewport->PlatformHandleRaw;
#elif __linux
    //SDL backend stores window not as SDL_Window*, but a uint32 id for some reason
    SDL_Window* window = SDL_GetWindowFromID((intptr_t)(viewport->PlatformHandle));
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    return reinterpret_cast<void*>(wmInfo.info.x11.window);
#else
    return nullptr;
#endif
}
void debugPrint(std::string& message,slag::SlagInitDetails::DebugLevel debugLevel,int32_t messageID)
{
    std::cout <<message<<std::endl;
}
int main()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }
    if(slag::SlagLib::initialize({.backend = slag::VULKAN,.debug=true,.slagDebugHandler=debugPrint})!= true)
    {
        printf("Error: Unable to initialize slag");
        return -1;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    SDL_Window* window = SDL_CreateWindow("Slag Dear IMGUI test",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,800,500,SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    slag::PlatformData pd{};

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
#ifdef _WIN32
    pd.platform = slag::PlatformData::WIN_32;
    pd.data.win32.hwnd = wmInfo.info.win.window;
    pd.data.win32.hinstance = wmInfo.info.win.hinstance;
#elif __linux
    pd.platform = slag::PlatformData::X11;
    pd.data.x11.window = wmInfo.info.x11.window;
    pd.data.x11.display = wmInfo.info.x11.display;
#endif

    int w,h;
    SDL_GetWindowSize(window,&w,&h);
    const slag::Pixels::Format BACK_BUFFER_FORMAT = slag::Pixels::B8G8R8A8_UNORM_SRGB;
    auto swapchain = slag::Swapchain::newSwapchain(pd, w, h, 3, slag::Swapchain::PresentMode::MAILBOX, BACK_BUFFER_FORMAT,generateDefaultFrameResources);
    auto renderQueue = slag::SlagLib::graphicsCard()->graphicsQueue();
    slag::ShaderModule modules[2] =
    {
        slag::ShaderModule(slag::ShaderStageFlags::VERTEX,"dear-imgui-shaders/dear-imgui.vert.spv"),
        slag::ShaderModule(slag::ShaderStageFlags::FRAGMENT,"dear-imgui-shaders/dear-imgui.frag.spv")
    };

    slag::ShaderProperties shaderProperties;

    slag::VertexDescription vertexDescription(1);
    vertexDescription.add(slag::GraphicsTypes::VECTOR2, offsetof(ImDrawVert,pos),0);
    vertexDescription.add(slag::GraphicsTypes::VECTOR2, offsetof(ImDrawVert,uv),0);
    vertexDescription.add(slag::GraphicsTypes::UNSIGNED_INTEGER, offsetof(ImDrawVert,col),0);

    slag::FrameBufferDescription frameBufferDescription;
    frameBufferDescription.addColorTarget(BACK_BUFFER_FORMAT);

    auto shader = slag::ShaderPipeline::newShaderPipeline(modules,2, nullptr,0,shaderProperties,&vertexDescription,frameBufferDescription);
    auto sampler = slag::SamplerBuilder().newSampler();

    ImGui_ImplSDL2_InitForOther(window);
    ImGui_ImplSlag_Init(swapchain,pd,extractNativeWindowHandle,shader,sampler,BACK_BUFFER_FORMAT);

    bool keepWindowOpen = true;
    while(keepWindowOpen)
    {
        SDL_Event e;
        int width, height;
        while(SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);

            switch (e.type)
            {
                case SDL_QUIT:
                    keepWindowOpen=false;
                    break;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == SDL_GetWindowID(window)) {
                        keepWindowOpen = false; // Or manually trigger SDL_QUIT
                    }
                    if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESTORED)
                    {
                        SDL_GetWindowSize(window, &width, &height);
                        swapchain->resize(width,height);
                    }
                    else if(e.window.event == SDL_WINDOWEVENT_MINIMIZED)
                    {
                        swapchain->resize(0,0);
                    }
                    break;
            }

        }

        if(auto frame = swapchain->next())
        {
            auto frameResources = static_cast<DefaultFrameResources*>(frame->resources);
            auto commandBuffer = frameResources->commandBuffer;
            auto descriptorPool = frameResources->descriptorPool;
            auto renderBuffer = frame->backBuffer();

            descriptorPool->reset();
            commandBuffer->begin();
            commandBuffer->bindDescriptorPool(descriptorPool);
            // Start the Dear ImGui frame
            ImGui_ImplSlag_NewFrame(descriptorPool);
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            commandBuffer->setViewPort(0,0,frame->backBuffer()->width(),frame->backBuffer()->height(),1,0);
            commandBuffer->setScissors({{0,0},{frame->backBuffer()->width(),frame->backBuffer()->height()}});

            commandBuffer->insertBarrier(
                    slag::ImageBarrier
                    {
                        .texture=renderBuffer,
                        .oldLayout=slag::Texture::UNDEFINED,
                        .newLayout=slag::Texture::RENDER_TARGET,
                        .accessBefore=slag::BarrierAccessFlags::NONE,
                        .accessAfter=slag::BarrierAccessFlags::SHADER_WRITE,
                        .syncBefore=slag::PipelineStageFlags::NONE,
                        .syncAfter=slag::PipelineStageFlags::FRAGMENT_SHADER
                    });

            slag::Attachment attachment{.texture=renderBuffer,.layout=slag::Texture::RENDER_TARGET,.clearOnLoad=true,.clear={.color={1.0f,0.0f,0.0f,1.0f}}};
            commandBuffer->beginRendering(&attachment,1, nullptr,{.offset={0,0},.extent={renderBuffer->width(),renderBuffer->height()}});

            ImGui::ShowDemoWindow();

            ImGui::Render();
            ImGui_ImplSlag_RenderDrawData(ImGui::GetDrawData(),commandBuffer);
            commandBuffer->endRendering();
            commandBuffer->insertBarrier(
                slag::ImageBarrier
                        {
                            .texture=renderBuffer,
                            .oldLayout=slag::Texture::RENDER_TARGET,
                            .newLayout=slag::Texture::PRESENT,
                            .accessBefore=slag::BarrierAccessFlags::SHADER_WRITE,
                            .accessAfter=slag::BarrierAccessFlags::NONE,
                            .syncBefore=slag::PipelineStageFlags::FRAGMENT_SHADER,
                            .syncAfter=slag::PipelineStageFlags::NONE
                        });
            commandBuffer->end();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }

            renderQueue->submit(&commandBuffer,1, nullptr,0, nullptr,0,frame);

        }


    }

    ImGui_ImplSlag_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    delete sampler;
    delete shader;
    delete swapchain;

    slag::SlagLib::cleanup();
    SDL_DestroyWindow(window);
    return 0;
}