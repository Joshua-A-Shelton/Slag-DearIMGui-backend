#include <iostream>
#include "imgui_impl_slag.h"

class ImGuiFrameResources: public slag::FrameResources
{
public:
    slag::CommandBuffer* commandBuffer = nullptr;
    ImGuiFrameResources()
    {
        commandBuffer = slag::CommandBuffer::newCommandBuffer(slag::GpuQueue::GRAPHICS);
    }
    ~ImGuiFrameResources()override
    {
        if(commandBuffer)
        {
            delete commandBuffer;
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

slag::FrameResources* ImGui_Slag_CreateFrameResources(size_t frameIndex, slag::Swapchain* swapchain)
{
    return new ImGuiFrameResources();
}

void ImGui_Slag_CreateWindow(ImGuiViewport* viewport)
{
    ImGuiIO& io = ImGui::GetIO();
    auto slagData = static_cast<ImGui_ImplSlag_Data*>(io.BackendRendererUserData);

    auto handle = viewport->PlatformHandleRaw;
    auto pd = slagData->platformData;
    slag::PlatformData platformData{};

    platformData.platform = pd.platform;
    switch (pd.platform)
    {

        case slag::PlatformData::WIN_32:
            platformData.data.win32 = {.hwnd = handle, .hinstance = pd.data.win32.hinstance};
            break;
        case slag::PlatformData::X11:
            platformData.data.x11 = {.window = handle, .display = pd.data.x11.display};
            break;
        case slag::PlatformData::WAYLAND:
            platformData.data.wayland = {.surface = handle, .display = pd.data.wayland.surface};
            break;
    }

    auto viewportData = new ImGui_ImplSlag_ViewportData();
    viewportData->swapchain = slag::Swapchain::newSwapchain(platformData,viewport->Size.x,viewport->Size.y,3,slag::Swapchain::MAILBOX,slagData->backBufferFormat,ImGui_Slag_CreateFrameResources);
    viewport->RendererUserData = viewportData;
}
void ImGui_Slag_DestroyWindow(ImGuiViewport* viewport)
{
    auto viewportData = static_cast<ImGui_ImplSlag_ViewportData*>(viewport->RendererUserData);
    delete viewportData;
    viewport->RendererUserData = nullptr;
}
void ImGui_Slag_SetWindowSize(ImGuiViewport* viewport, ImVec2 newSize)
{
    auto viewportData = static_cast<ImGui_ImplSlag_ViewportData*>(viewport->RendererUserData);
    viewportData->swapchain->resize(newSize.x,newSize.y);
}
void ImGui_Slag_RenderWindow(ImGuiViewport* viewport, void* unknown)
{
    ImGuiIO& io = ImGui::GetIO();

    auto viewportData = static_cast<ImGui_ImplSlag_ViewportData*>(viewport->RendererUserData);
    auto frame = viewportData->swapchain->currentFrame();
    if(frame)
    {
        auto rendererData = static_cast<ImGui_ImplSlag_Data*>(io.BackendRendererUserData);
        auto resources = static_cast<ImGuiFrameResources*>(frame->resources);
        auto commandBuffer = resources->commandBuffer;
        auto renderBuffer = frame->backBuffer();

        commandBuffer->begin();
        commandBuffer->bindDescriptorPool(rendererData->descriptorPool);
        commandBuffer->insertBarrier(
                {.texture=renderBuffer,
                 .oldLayout=slag::Texture::UNDEFINED,
                 .newLayout=slag::Texture::RENDER_TARGET,
                 .accessBefore=slag::BarrierAccessFlags::NONE,
                 .accessAfter=slag::BarrierAccessFlags::COLOR_ATTACHMENT_WRITE,
                 .syncBefore = slag::PipelineStageFlags::NONE,
                 .syncAfter = slag::PipelineStageFlags::COLOR_ATTACHMENT
                }
         );

        slag::Attachment attachment{.texture=renderBuffer,.layout=slag::Texture::RENDER_TARGET,.clearOnLoad=true,.clear={.color={0.0f,0.0f,0.0f,1.0f}}};
        commandBuffer->beginRendering(&attachment,1, nullptr,{.offset={0,0},.extent={renderBuffer->width(),renderBuffer->height()}});
        commandBuffer->endRendering();

        ImGui_ImplSlag_RenderDrawData(viewport->DrawData,commandBuffer);

        commandBuffer->insertBarrier(
                {.texture=frame->backBuffer(),
                        .oldLayout=slag::Texture::RENDER_TARGET,
                        .newLayout=slag::Texture::PRESENT,
                        .accessBefore=slag::BarrierAccessFlags::COLOR_ATTACHMENT_WRITE,
                        .accessAfter=slag::BarrierAccessFlags::NONE,
                        .syncBefore = slag::PipelineStageFlags::COLOR_ATTACHMENT,
                        .syncAfter = slag::PipelineStageFlags::NONE
                }
        );

        commandBuffer->end();

        slag::SlagLib::graphicsCard()->graphicsQueue()->submit(&commandBuffer, 1, nullptr, 0, nullptr, 0, frame);
    }
}
void ImGui_Slag_SwapBuffers(ImGuiViewport* viewport, void* unknown)
{
    auto viewportData = static_cast<ImGui_ImplSlag_ViewportData*>(viewport->RendererUserData);
    viewportData->swapchain->next();
}
bool ImGui_ImplSlag_Init(slag::Swapchain* mainSwapchain, slag::PlatformData platformData, slag::ShaderPipeline* shaderPipeline, slag::Sampler* sampler, slag::Pixels::Format backBufferFormat)
{
    //set backend data
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_slag";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    auto backendData = new ImGui_ImplSlag_Data();
    io.BackendRendererUserData = backendData;

    backendData->shaderPipeline = shaderPipeline;
    backendData->sampler = sampler;
    backendData->platformData = platformData;
    backendData->backBufferFormat = backBufferFormat;

    //create dear imgui managed resources(just the texture, the descriptor bundle has to have a current descriptor pool, which requires an active frame)
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t upload_size = width * height * 4 * sizeof(char);
    backendData->fontsTexture = slag::Texture::newTexture(pixels,slag::Pixels::R8G8B8A8_UNORM,width,height,1,slag::TextureUsageFlags::SAMPLED_IMAGE,slag::Texture::SHADER_RESOURCE);

    auto viewportData = new ImGui_ImplSlag_ViewportData();
    viewportData->swapchain = mainSwapchain;
    viewportData->outsideManaged = true;

    auto mainViewport = ImGui::GetMainViewport();
    mainViewport->RendererUserData = viewportData;

    ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
    platformIo.Renderer_CreateWindow = ImGui_Slag_CreateWindow;
    platformIo.Renderer_DestroyWindow = ImGui_Slag_DestroyWindow;
    platformIo.Renderer_SetWindowSize = ImGui_Slag_SetWindowSize;
    platformIo.Renderer_RenderWindow = ImGui_Slag_RenderWindow;
    platformIo.Renderer_SwapBuffers = ImGui_Slag_SwapBuffers;

    return true;
}

void ImGui_ImplSlag_Shutdown()
{
    ImGuiIO& io = ImGui::GetIO();
    auto backend = static_cast<ImGui_ImplSlag_Data*>(io.BackendRendererUserData);
    //clean up IMGUI managed resources
    delete backend->fontsTexture;
    delete backend->fontsTextureBundle;

    delete backend;
    io.BackendRendererUserData = nullptr;
}

void ImGui_ImplSlag_NewFrame(slag::DescriptorPool* framePool)
{
    ImGuiIO& io = ImGui::GetIO();
    auto rendererData = static_cast<ImGui_ImplSlag_Data*>(io.BackendRendererUserData);
    rendererData->descriptorPool = framePool;
    if(rendererData->fontsTextureBundle== nullptr)
    {
        rendererData->fontsTextureBundle = new slag::DescriptorBundle(framePool->makeBundle(rendererData->shaderPipeline->descriptorGroup(0)));
    }
    else
    {
        *(rendererData->fontsTextureBundle) = framePool->makeBundle(rendererData->shaderPipeline->descriptorGroup(0));

    }
    rendererData->fontsTextureBundle->setSamplerAndTexture(0,0,rendererData->fontsTexture, slag::Texture::SHADER_RESOURCE,rendererData->sampler);
    io.Fonts->SetTexID((ImTextureID)rendererData->fontsTextureBundle);
}
void ImGui_ImplSlag_SetupRenderState(ImDrawData* draw_data, slag::ShaderPipeline* pipeline, slag::CommandBuffer* commandBuffer, slag::Buffer* vertexBuffer, slag::Buffer* indexBuffer, uint32_t frameBufferWidth, uint32_t frameBufferHeight)
{
    ImGuiIO& io = ImGui::GetIO();
    auto slagData = static_cast<ImGui_ImplSlag_Data*>(io.BackendRendererUserData);

    commandBuffer->bindGraphicsShader(slagData->shaderPipeline);


    // Bind Vertex And Index Buffer:
    if (draw_data->TotalVtxCount > 0)
    {
        size_t offset = 0;
        size_t stride = sizeof(ImDrawVert);
        size_t size = vertexBuffer->size();
        commandBuffer->bindVertexBuffers(0,&vertexBuffer,&offset,&size,&stride,1);
        commandBuffer->bindIndexBuffer(indexBuffer,sizeof(ImDrawIdx) == 2 ? slag::Buffer::UINT16: slag::Buffer::UINT32,0);
    }

    commandBuffer->setViewPort(0,0,frameBufferWidth,frameBufferHeight,1,0);

    {
        float scale[2];
        scale[0] = 2.0f / draw_data->DisplaySize.x;
        scale[1] = 2.0f / draw_data->DisplaySize.y;
        float translate[2];
        translate[0] =  -1.0f - draw_data->DisplayPos.x * scale[0];
        translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];
        commandBuffer->pushConstants(pipeline,slag::ShaderStageFlags::VERTEX,0,sizeof(float)*2,scale);
        commandBuffer->pushConstants(pipeline,slag::ShaderStageFlags::VERTEX,sizeof(float)*2,sizeof(float)*2,translate);
        auto drawList = draw_data->CmdLists.Data[0];

    }
}
void ImGui_ImplSlag_RenderDrawData(ImDrawData* draw_data, slag::CommandBuffer* commandBuffer)
{
    ImGuiIO& io = ImGui::GetIO();
    auto rendererData = static_cast<ImGui_ImplSlag_Data*>(io.BackendRendererUserData);
    auto rendererViewportData = static_cast<ImGui_ImplSlag_ViewportData*>(draw_data->OwnerViewport->RendererUserData);
    auto currentIndex = rendererViewportData->swapchain->currentFrameIndex();
    auto shader = rendererData->shaderPipeline;
    auto sampler = rendererData->sampler;
    if(draw_data->TotalVtxCount > 0)
    {
        if(rendererViewportData->drawDataArrays[currentIndex] == nullptr)
        {
            //delete existing arrays
            delete rendererViewportData->drawDataArrays[currentIndex];
            delete rendererViewportData->drawDataIndexArrays[currentIndex];
            //create new arrays that will fit the data
            rendererViewportData->drawDataArrays[currentIndex]= slag::Buffer::newBuffer(draw_data->TotalVtxCount*sizeof(ImDrawVert),slag::Buffer::CPU_AND_GPU,slag::Buffer::DATA_BUFFER);
            rendererViewportData->drawDataIndexArrays[currentIndex]= slag::Buffer::newBuffer(draw_data->TotalIdxCount*sizeof(ImDrawIdx),slag::Buffer::CPU_AND_GPU,slag::Buffer::DATA_BUFFER);
        }

        if( rendererViewportData->drawDataArrays[currentIndex]->size()< draw_data->TotalVtxCount*sizeof(ImDrawVert))
        {
            delete rendererViewportData->drawDataArrays[currentIndex];
            rendererViewportData->drawDataArrays[currentIndex]= slag::Buffer::newBuffer(draw_data->TotalVtxCount*sizeof(ImDrawVert),slag::Buffer::CPU_AND_GPU,slag::Buffer::DATA_BUFFER);
        }
        if(rendererViewportData->drawDataIndexArrays[currentIndex]->size()< draw_data->TotalIdxCount*sizeof(ImDrawIdx))
        {
            delete rendererViewportData->drawDataIndexArrays[currentIndex];
            rendererViewportData->drawDataIndexArrays[currentIndex]= slag::Buffer::newBuffer(draw_data->TotalIdxCount*sizeof(ImDrawIdx),slag::Buffer::CPU_AND_GPU,slag::Buffer::DATA_BUFFER);
        }
        //copy draw data into buffers
        auto vertexBuffer = rendererViewportData->drawDataArrays[currentIndex];
        auto indexBuffer = rendererViewportData->drawDataIndexArrays[currentIndex];
        size_t vertexOffset = 0;
        size_t indexOffset = 0;
        for(int i=0; i<draw_data->CmdListsCount; i++)
        {
            const ImDrawList* draw_list = draw_data->CmdLists[i];
            auto vsize = draw_list->VtxBuffer.Size*sizeof(ImDrawVert);
            auto isize = draw_list->IdxBuffer.Size*sizeof(ImDrawIdx);
            vertexBuffer->update(vertexOffset,draw_list->VtxBuffer.Data,vsize);
            indexBuffer->update(indexOffset,draw_list->IdxBuffer.Data,isize);
            vertexOffset+=vsize;
            indexOffset+=isize;
        }
        auto frameBufferWidth = rendererViewportData->swapchain->width();
        auto frameBufferHeight = rendererViewportData->swapchain->height();

        ImGui_ImplSlag_SetupRenderState(draw_data,shader,commandBuffer,vertexBuffer,indexBuffer,frameBufferWidth,frameBufferHeight);
        // Setup render state structure (for callbacks and custom texture bindings)
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        ImGui_ImplSlag_RenderState render_state;
        render_state.commandBuffer = commandBuffer;
        render_state.shader = shader;
        platform_io.Renderer_RenderState = &render_state;

        // Will project scissor/clipping rectangles into framebuffer space
        ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
        ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

        size_t vertexDrawOffset = 0;
        size_t indexDrawOffset = 0;
        for(int i=0; i< draw_data->CmdListsCount; i++)
        {
            const ImDrawList* draw_list = draw_data->CmdLists[i];
            for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
            {
                const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
                if (pcmd->UserCallback != nullptr)
                {
                    // User callback, registered via ImDrawList::AddCallback()
                    // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                    if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    {
                        ImGui_ImplSlag_SetupRenderState(draw_data,shader,commandBuffer,vertexBuffer,indexBuffer,frameBufferWidth,frameBufferHeight);
                    }
                    else
                    {
                        pcmd->UserCallback(draw_list,pcmd);
                    }
                }
                else
                {
                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                    ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                    // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                    if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                    if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                    if (clip_max.x > frameBufferWidth) { clip_max.x = (float)frameBufferWidth; }
                    if (clip_max.y > frameBufferHeight) { clip_max.y = (float)frameBufferHeight; }
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                        continue;

                    // Apply scissor/clipping rectangle
                    slag::Rectangle scissor;
                    scissor.offset.x = (int32_t)(clip_min.x);
                    scissor.offset.y = (int32_t)(clip_min.y);
                    scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                    scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
                    commandBuffer->setScissors(scissor);


                    // Bind DescriptorSet with font or user texture
                    auto descriptorBundle = (slag::DescriptorBundle*)pcmd->GetTexID();
                    commandBuffer->bindGraphicsDescriptorBundle(shader,0,*descriptorBundle);
                    // Draw
                    commandBuffer->drawIndexed(pcmd->ElemCount,1,pcmd->IdxOffset+indexDrawOffset,pcmd->VtxOffset+vertexDrawOffset,0);
                }
            }
            indexDrawOffset += draw_list->IdxBuffer.Size;
            vertexDrawOffset += draw_list->VtxBuffer.Size;
        }

        platform_io.Renderer_RenderState = nullptr;

        slag::Rectangle scissor = { { 0, 0 }, { frameBufferWidth, frameBufferHeight } };
        commandBuffer->setScissors(scissor);
    }

}
