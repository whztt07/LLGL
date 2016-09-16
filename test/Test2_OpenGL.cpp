/*
 * Test2_OpenGL.cpp
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include <LLGL/LLGL.h>
#include <Gauss/Gauss.h>
#include <memory>
#include <iostream>
#include <string>
#include <sstream>


//#define TEST_RENDER_TARGET
//#define TEST_QUERY
//#define TEST_STORAGE_BUFFER


int main()
{
    try
    {
        // Load render system module
        std::shared_ptr<LLGL::RenderingProfiler> profiler;// = std::make_shared<LLGL::RenderingProfiler>();

        auto renderer = LLGL::RenderSystem::Load("OpenGL", profiler.get());

        // Create render context
        LLGL::RenderContextDescriptor contextDesc;

        contextDesc.videoMode.resolution    = { 800, 600 };
        //contextDesc.videoMode.fullscreen    = true;

        contextDesc.antiAliasing.enabled    = true;
        contextDesc.antiAliasing.samples    = 8;

        contextDesc.vsync.enabled           = true;

        #if 0
        contextDesc.debugCallback = [](const std::string& type, const std::string& message)
        {
            std::cout << type << ':' << std::endl << "  " << message << std::endl;
        };
        #endif

        #ifdef __linux__
        
        auto context = renderer->CreateRenderContext(contextDesc);
        
        auto window = &(context->GetWindow());

        #else
        
        LLGL::WindowDescriptor windowDesc;
        {
            windowDesc.size             = contextDesc.videoMode.resolution;
            windowDesc.borderless       = contextDesc.videoMode.fullscreen;
            windowDesc.centered         = !contextDesc.videoMode.fullscreen;
            windowDesc.resizable        = true;
        }
        auto window = std::shared_ptr<LLGL::Window>(LLGL::Window::Create(windowDesc));

        auto context = renderer->CreateRenderContext(contextDesc, window);
        
        #endif

        window->Show();

        auto renderCaps = renderer->QueryRenderingCaps();

        auto shadingLang = renderer->QueryShadingLanguage();

        // Show renderer info
        auto info = renderer->QueryRendererInfo();

        std::cout << "Renderer:         " << info[LLGL::RendererInfo::Version] << std::endl;
        std::cout << "Vendor:           " << info[LLGL::RendererInfo::Vendor] << std::endl;
        std::cout << "Hardware:         " << info[LLGL::RendererInfo::Hardware] << std::endl;
        std::cout << "Shading Language: " << info[LLGL::RendererInfo::ShadingLanguageVersion] << std::endl;

        // Setup window title
        auto title = "LLGL Test 2 ( " + renderer->GetName() + " )";
        window->SetTitle(std::wstring(title.begin(), title.end()));

        // Setup input controller
        auto input = std::make_shared<LLGL::Input>();
        window->AddEventListener(input);

        class ResizeEventHandler : public LLGL::Window::EventListener
        {
        public:
            ResizeEventHandler(LLGL::RenderContext* context) :
                context_( context )
            {
            }
            void OnResize(LLGL::Window& sender, const LLGL::Size& clientAreaSize) override
            {
                auto videoMode = context_->GetVideoMode();
                videoMode.resolution = clientAreaSize;

                context_->SetVideoMode(videoMode);
                
                LLGL::Viewport viewport;
                {
                    viewport.width  = static_cast<float>(videoMode.resolution.x);
                    viewport.height = static_cast<float>(videoMode.resolution.y);
                }
                context_->SetViewports({ viewport });
            }
        private:
            LLGL::RenderContext* context_;
        };

        auto resizeEventHandler = std::make_shared<ResizeEventHandler>(context);
        window->AddEventListener(resizeEventHandler);

        // Create vertex buffer
        auto& vertexBuffer = *renderer->CreateVertexBuffer();

        LLGL::VertexFormat vertexFormat;
        vertexFormat.AddAttribute("texCoord", LLGL::DataType::Float32, 2);
        vertexFormat.AddAttribute("position", LLGL::DataType::Float32, 2);

        const Gs::Vector2f vertices[] =
        {
            { 0, 0 }, { 110, 100 },
            { 0, 0 }, { 200, 100 },
            { 0, 0 }, { 200, 200 },
            { 0, 0 }, { 100, 200 },
        };
        
        renderer->SetupVertexBuffer(vertexBuffer, vertices, sizeof(vertices), LLGL::BufferUsage::Static, vertexFormat);

        // Create vertex shader
        auto& vertShader = *renderer->CreateShader(LLGL::ShaderType::Vertex);

        std::string shaderSource =
        (
            #ifdef TEST_STORAGE_BUFFER
            "#version 430\n"
            #else
            "#version 130\n"
            #endif
            "uniform mat4 projection;\n"
            #ifdef TEST_STORAGE_BUFFER
            "layout(std430) buffer outputBuffer {\n"
            "    float v[4];\n"
            "} outputData;\n"
            #endif
            "in vec2 position;\n"
            "out vec2 vertexPos;\n"
            "void main() {\n"
            "    gl_Position = projection * vec4(position, 0.0, 1.0);\n"
            "    vertexPos = (position - vec2(125, 125))*vec2(0.02);\n"
            #ifdef TEST_STORAGE_BUFFER
            "    outputData.v[gl_VertexID] = vertexPos.x;\n"
            #endif
            "}\n"
        );

        if (!vertShader.Compile(shaderSource))
            std::cerr << vertShader.QueryInfoLog() << std::endl;

        // Create fragment shader
        auto& fragShader = *renderer->CreateShader(LLGL::ShaderType::Fragment);

        shaderSource =
        (
            "#version 130\n"
            "out vec4 fragColor;\n"
            "uniform sampler2D tex;\n"
            "uniform vec4 color;\n"
            "in vec2 vertexPos;\n"
            "void main() {\n"
            "    fragColor = texture(tex, vertexPos) * color;\n"
            "}\n"
        );

        if (!fragShader.Compile(shaderSource))
            std::cerr << fragShader.QueryInfoLog() << std::endl;

        // Create shader program
        auto& shaderProgram = *renderer->CreateShaderProgram();

        shaderProgram.AttachShader(vertShader);
        shaderProgram.AttachShader(fragShader);

        shaderProgram.BindVertexAttributes(vertexFormat.GetAttributes());

        if (!shaderProgram.LinkShaders())
            std::cerr << shaderProgram.QueryInfoLog() << std::endl;

        auto vertAttribs = shaderProgram.QueryVertexAttributes();

        // Set shader uniforms
        auto projection = Gs::ProjectionMatrix4f::Planar(
            static_cast<Gs::Real>(contextDesc.videoMode.resolution.x),
            static_cast<Gs::Real>(contextDesc.videoMode.resolution.y)
        );

        auto uniformSetter = shaderProgram.LockShaderUniform();
        if (uniformSetter)
        {
            uniformSetter->SetUniform("projection", projection);
            uniformSetter->SetUniform("color", Gs::Vector4f(1, 1, 1, 1));
        }
        shaderProgram.UnlockShaderUniform();

        #if 0
        // Create constant buffer
        LLGL::ConstantBuffer* projectionBuffer = nullptr;

        for (const auto& desc : shaderProgram.QueryConstantBuffers())
        {
            if (desc.name == "Matrices")
            {
                projectionBuffer = renderer->CreateConstantBuffer();

                renderer->WriteConstantBuffer(*projectionBuffer, &projection, sizeof(projection), LLGL::BufferUsage::Static);

                unsigned int bindingIndex = 2; // the 2 is just for testing
                shaderProgram.BindConstantBuffer(desc.name, bindingIndex);
                context->SetConstantBuffer(*projectionBuffer, bindingIndex);
            }
        }
        #endif

        for (const auto& desc : shaderProgram.QueryUniforms())
        {
            std::cout << "uniform: name = \"" << desc.name << "\", location = " << desc.location << ", size = " << desc.size << std::endl;
        }

        // Create texture
        auto& texture = *renderer->CreateTexture();

        LLGL::ColorRGBub image[4] =
        {
            { 255, 0, 0 },
            { 0, 255, 0 },
            { 0, 0, 255 },
            { 255, 0, 255 }
        };

        LLGL::ImageDataDescriptor textureData;
        {
            textureData.dataFormat  = LLGL::ColorFormat::RGB;
            textureData.dataType    = LLGL::DataType::UInt8;
            textureData.data        = image;
        }
        renderer->SetupTexture2D(texture, LLGL::TextureFormat::RGBA, { 2, 2 }, &textureData); // create 2D texture
        //renderer->SetupTexture1D(texture, LLGL::TextureFormat::RGBA, 4, &textureData); // immediate change to 1D texture

        #ifndef __linux__
        context->GenerateMips(texture);
        #endif

        auto textureDesc = renderer->QueryTextureDescriptor(texture);

        // Create render target
        LLGL::RenderTarget* renderTarget = nullptr;
        LLGL::Texture* renderTargetTex = nullptr;

        #ifdef TEST_RENDER_TARGET
        
        renderTarget = renderer->CreateRenderTarget(8);

        auto renderTargetSize = contextDesc.videoMode.resolution;

        renderTargetTex = renderer->CreateTexture();
        renderer->SetupTexture2D(*renderTargetTex, LLGL::TextureFormat::RGBA8, renderTargetSize);

        //auto numMips = LLGL::NumMipLevels({ renderTargetSize.x, renderTargetSize.y, 1 });

        //renderTarget->AttachDepthBuffer(renderTargetSize);
        renderTarget->AttachTexture2D(*renderTargetTex);

        #endif
        
        // Create graphics pipeline
        LLGL::GraphicsPipelineDescriptor pipelineDesc;
        {
            pipelineDesc.rasterizer.multiSampleEnabled = true;

            pipelineDesc.shaderProgram = &shaderProgram;

            LLGL::BlendTargetDescriptor blendDesc;
            {
                blendDesc.destColor = LLGL::BlendOp::Zero;
            }
            pipelineDesc.blend.targets.push_back(blendDesc);
        }
        auto& pipeline = *renderer->CreateGraphicsPipeline(pipelineDesc);

        // Create sampler
        LLGL::SamplerDescriptor samplerDesc;
        {
            samplerDesc.magFilter = LLGL::TextureFilter::Nearest;
            samplerDesc.minFilter = LLGL::TextureFilter::Linear;
            samplerDesc.textureWrapU = LLGL::TextureWrap::Border;
            samplerDesc.textureWrapV = LLGL::TextureWrap::Border;
            samplerDesc.borderColor = LLGL::ColorRGBAf(0, 0.7f, 0.5f, 1);
        }
        auto& sampler = *renderer->CreateSampler(samplerDesc);

        //#ifndef __linux__
        context->SetSampler(sampler, 0);
        //#endif
        
        //context->SetViewports({ LLGL::Viewport(0, 0, 300, 300) });

        #ifdef TEST_QUERY
        auto query = renderer->CreateQuery(LLGL::QueryType::SamplesPassed);
        bool hasQueryResult = false;
        #endif

        #ifdef TEST_STORAGE_BUFFER
        
        LLGL::StorageBuffer* storage = nullptr;

        if (renderCaps.hasStorageBuffers)
        {
            storage = renderer->CreateStorageBuffer();
            renderer->SetupStorageBuffer(*storage, nullptr, sizeof(float)*4, LLGL::BufferUsage::Static);
            shaderProgram.BindStorageBuffer("outputBuffer", 0);
            context->SetStorageBuffer(0, *storage);

            auto storeBufferDescs = shaderProgram.QueryStorageBuffers();
            for (const auto& desc : storeBufferDescs)
                std::cout << "storage buffer: name = \"" << desc.name << '\"' << std::endl;
        }

        #endif

        // Main loop
        while (window->ProcessEvents() && !input->KeyDown(LLGL::Key::Escape))
        {
            if (profiler)
                profiler->ResetCounters();

            context->SetClearColor(LLGL::ColorRGBAf(0.3f, 0.3f, 1));
            context->ClearBuffers(LLGL::ClearBuffersFlags::Color);

            context->SetDrawMode(LLGL::DrawMode::TriangleFan);

            auto uniformSetter = shaderProgram.LockShaderUniform();
            if (uniformSetter)
            {
                auto projection = Gs::ProjectionMatrix4f::Planar(
                    static_cast<Gs::Real>(context->GetVideoMode().resolution.x),
                    static_cast<Gs::Real>(context->GetVideoMode().resolution.y)
                );
                uniformSetter->SetUniform("projection", projection);
            }
            shaderProgram.UnlockShaderUniform();
            
            context->SetGraphicsPipeline(pipeline);
            context->SetVertexBuffer(vertexBuffer);

            if (renderTarget && renderTargetTex)
            {
                context->SetRenderTarget(*renderTarget);
                context->SetClearColor({ 1, 1, 1, 1 });
                context->ClearBuffers(LLGL::ClearBuffersFlags::Color);
            }

            #ifndef __linux__
            
            // Switch fullscreen mode
            if (input->KeyDown(LLGL::Key::Return))
            {
                windowDesc.borderless = !windowDesc.borderless;

                /*auto videoMode = contextDesc.videoMode;
                videoMode.fullscreen = windowDesc.borderless;
                LLGL::Desktop::SetVideoMode(videoMode);*/

                windowDesc.centered = true;//!windowDesc.borderless;
                windowDesc.position = { 0, 0 };
                windowDesc.resizable = true;
                windowDesc.visible = true;
                window->SetDesc(windowDesc);

                context->SetVideoMode(contextDesc.videoMode);

                LLGL::Viewport viewport;
                {
                    viewport.width  = static_cast<float>(contextDesc.videoMode.resolution.x);
                    viewport.height = static_cast<float>(contextDesc.videoMode.resolution.y);
                }
                context->SetViewports({ viewport });
            }
            
            #endif

            #ifdef TEST_QUERY
            
            if (!hasQueryResult)
                context->BeginQuery(*query);

            #endif

            context->SetTexture(texture, 0);
            context->Draw(4, 0);
            
            #ifdef TEST_STORAGE_BUFFER
            
            if (renderCaps.hasStorageBuffers)
            {
                static bool outputShown;
                if (!outputShown)
                {
                    outputShown = true;
                    auto outputData = context->MapStorageBuffer(*storage, LLGL::BufferCPUAccess::ReadOnly);
                    {
                        auto v = reinterpret_cast<Gs::Vector4f*>(outputData);
                        std::cout << "storage buffer output: " << *v << std::endl;
                    }
                    context->UnmapStorageBuffer();
                }
            }

            #endif

            #ifdef TEST_QUERY

            if (!hasQueryResult)
            {
                context->EndQuery(*query);
                hasQueryResult = true;
            }

            std::uint64_t result = 0;
            if (context->QueryResult(*query, result))
            {
                static std::uint64_t prevResult;
                if (prevResult != result)
                {
                    prevResult = result;
                    std::cout << "query result = " << result << std::endl;
                }
                hasQueryResult = false;
            }

            #endif

            if (renderTarget && renderTargetTex)
            {
                context->UnsetRenderTarget();
                context->SetTexture(*renderTargetTex, 0);
                context->Draw(4, 0);
            }

            context->Present();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        #ifdef _WIN32
        system("pause");
        #endif
    }

    return 0;
}
