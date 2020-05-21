// dear imgui: standalone example application for SDL2 + DirectX 11
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

#include <d3d11.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include "../../../shared_libs/dear_imgui/imgui.h"
#include "../../../shared_libs/dear_imgui/imgui_impl_sdl.h"
#include "../../../shared_libs/dear_imgui/imgui_impl_dx11.h"

// Data
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

static const char* WINDOW_NAME = "Knot: UE4-AutomationTool-Commandline Builder";
constexpr uint32_t APP_WIDTH = 700;
constexpr uint32_t APP_HEIGHT = 800;

enum class BuildConfig : uint8_t
{
	Debug,
	Development,
	Test,
	Shipping,

	Count
};

constexpr char* BuildConfigNames[static_cast<uint8_t>(BuildConfig::Count)] = 
{ 
	"Debug",
	"Development",
	"Test",
	"Shipping"
};

enum class TargetPlatform : uint8_t
{
	Win64,
	PS4,
	XBoxOne,
	Switch,

	Count
};

constexpr char* TargetPlatformNames[static_cast<uint8_t>(TargetPlatform::Count)] = 
{ 
	"Win64",
	"PS4",
	"XboxOne",
	"Switch" 
};

void SerializeString(std::stringstream& ss, const char* data)
{
	if (data[0] == '\0')
	{
		ss << "EMPTY ";
	}
	else
	{
		ss << data << " ";
	}
}

void DeserializeString(std::stringstream& ss, char* data)
{
	std::string val;
	ss >> val;

	if (strcmp(val.c_str(), "EMPTY") == 0)
	{
		data[0] = '\0';
	}
	else
	{
		memcpy(data, val.c_str(), val.length());
	}
}

constexpr uint32_t MAX_STRING_OPTION_LENGTH = 66;

struct Command
{
	// general settings
	char engine_path[MAX_PATH] = { "\0" };
	char project_name[MAX_STRING_OPTION_LENGTH] = { "\0" };
	char project_path[MAX_PATH] = { "\0" };
	char title_id[MAX_STRING_OPTION_LENGTH] = { "\0" };

	// build options
	bool should_build = false;

	// cooking options
	bool should_cook = false;

	// stage options
	bool should_stage = false;
	bool use_pak_files = false;
	bool use_mutliple_chunks = false;

	// package options
	bool should_package = false;
	bool is_distribution = false;
	bool should_compress = false;

	// archive options
	bool should_archive = false;
	char archive_path[MAX_PATH] = { "\0" };

	BuildConfig build_config = BuildConfig::Debug;
	TargetPlatform target_platform = TargetPlatform::Win64;

	std::string serialize(void)
	{
		std::stringstream ss;
		SerializeString(ss, engine_path);
		SerializeString(ss, project_name);
		SerializeString(ss, project_path);
		SerializeString(ss, title_id);

		ss << should_build << " ";
		ss << should_cook << " ";
		ss << should_stage << " ";
		ss << use_pak_files << " ";
		ss << use_mutliple_chunks << " ";
		
		ss << should_package << " ";
		ss << is_distribution << " ";
		ss << should_compress << " ";
		
		ss << should_archive << " ";
		SerializeString(ss, archive_path);

		ss << static_cast<uint8_t>(build_config) << " ";
		ss << static_cast<uint8_t>(target_platform) << " ";

		return ss.str();
	}

	void deserialize(const std::string& data)
	{
		std::stringstream ss(data);
		DeserializeString(ss, engine_path);
		DeserializeString(ss, project_name);
		DeserializeString(ss, project_path);
		DeserializeString(ss, title_id);
		   
		ss >> should_build;
		ss >> should_cook;
		ss >> should_stage;
		ss >> use_pak_files;
		ss >> use_mutliple_chunks;

		ss >> should_package;
		ss >> is_distribution;
		ss >> should_compress;
		   
		ss >> should_archive;
		DeserializeString(ss, archive_path);
		  
		uint8_t config = 0u;
		ss >> config;
		build_config = static_cast<BuildConfig>(config);
		ss >> config;
		target_platform = static_cast<TargetPlatform>(config);
	}
};

const char* CombinedBuildOptions(const Command& command)
{
	return command.should_build ? "-build" : "-skipbuild";
}

const char* CombinedCookOptions(const Command& command)
{
	return command.should_cook ? "-cook" : "-skipcook";
}

const char* CombinedStageOptions(const Command& command)
{
	static char stage_options[29] = { "\0" };

	if (command.should_stage)
	{
		const char* pak = command.use_pak_files ? "-pak" : "";
		const char* chunks = command.use_mutliple_chunks ? "-manifests" : "";
		const char* compressed = command.should_compress ? "-compressed" : "";

		snprintf(stage_options, 29, "-stage %s %s %s", pak, chunks, compressed);
		
		return stage_options;
	}

	return "-skipstage";
}

const char* CombinedPackageOptions(const Command& command)
{
	if (command.should_package)
	{
		if (command.is_distribution)
		{
			return "-package -distribution";
		}

		return "-package";
	}

	return "";
}

const char* CombinedArchiveOptions(const Command& command)
{
	if (command.should_archive)
	{
		static char archive_options[MAX_PATH + 26] = { "\0" };
		snprintf(archive_options, MAX_PATH + 26, "-archive -archivepath=%s", command.archive_path);
		return archive_options;
	}

	return "";
}

const char* OptionalTitleId(const Command& command)
{
	if (command.title_id[0] != '/0')
	{
		static char archive_options[MAX_STRING_OPTION_LENGTH + 20] = { "\0" };
		snprintf(archive_options, MAX_STRING_OPTION_LENGTH + 20, "-TitleId=%s", command.title_id);
		return archive_options;
	}

	return "";
}

void NewCommand(Command& command)
{
	command = Command {};
}

void OpenCommand(Command& command, HWND ownerHwnd)
{
	OPENFILENAME ofn;
	wchar_t file_name[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = ownerHwnd;
	ofn.lpstrFile = file_name;

	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(file_name);
	ofn.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if(GetOpenFileName(&ofn) == TRUE)
	{
		std::ifstream save_file;
		save_file.open(ofn.lpstrFile, std::ios::in);
		std::string data;
		std::getline(save_file, data);
		command.deserialize(data);
		save_file.close();
	}
}

void SaveCommand(Command& command, HWND ownerHwnd)
{
	OPENFILENAME ofn;
	wchar_t file_name[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = ownerHwnd;
	ofn.lpstrFile = file_name;

	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(file_name);
	ofn.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if(GetSaveFileName(&ofn) == TRUE)
	{
		std::ofstream save_file;
		save_file.open(ofn.lpstrFile, std::ios::out);
		save_file << command.serialize();
		save_file.close();
	}
}

void SaveCommandStringToFile(const char* data, HWND ownerHwnd)
{
	OPENFILENAME ofn;
	wchar_t file_name[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = ownerHwnd;
	ofn.lpstrFile = file_name;

	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(file_name);
	ofn.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if(GetSaveFileName(&ofn) == TRUE)
	{
		std::ofstream save_file;
		save_file.open(ofn.lpstrFile, std::ios::out);
		save_file << data;
		save_file.close();
	}
}

void ShowAboutWindow()
{
	ImGui::Begin("About", nullptr);

	ImGui::End();
}

// Main code
int main(int, char**)
{
	::ShowWindow(::GetConsoleWindow(), SW_HIDE);

    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow(WINDOW_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, APP_WIDTH, APP_HEIGHT, window_flags);
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = (HWND)wmInfo.info.win.window;

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForD3D(window);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);

	Command command;

	ImGuiTreeNodeFlags header_flags = ImGuiTreeNodeFlags_DefaultOpen;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED && event.window.windowID == SDL_GetWindowID(window))
            {                
                // Release all outstanding references to the swap chain's buffers before resizing.
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

	    ImGuiWindowFlags img_window_flags = 0;
	    img_window_flags |= ImGuiWindowFlags_NoTitleBar;
	    img_window_flags |= ImGuiWindowFlags_NoResize;
	    img_window_flags |= ImGuiWindowFlags_NoScrollbar;
	    img_window_flags |= ImGuiWindowFlags_MenuBar;
	    img_window_flags |= ImGuiWindowFlags_NoMove;
	    img_window_flags |= ImGuiWindowFlags_NoCollapse;

	    // We specify a default position/size in case there's no data in the .ini file. Typically this isn't required! We only do it to make the Demo applications a little more welcoming.
	    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	    ImGui::SetNextWindowSize(ImVec2(APP_WIDTH, APP_HEIGHT), ImGuiCond_FirstUseEver);

	    // Main body of the Demo window starts here.
		ImGui::Begin("MainWindow", nullptr, img_window_flags);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New")) { NewCommand(command); }
				if (ImGui::MenuItem("Open")) { OpenCommand(command, hwnd); }
				if (ImGui::MenuItem("Save")) { SaveCommand(command, hwnd); }
			
				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("About")) { ShowAboutWindow(); }
			if (ImGui::MenuItem("Exit")) { return 0; }

			ImGui::EndMenuBar();
		}

		if (ImGui::CollapsingHeader("General options", header_flags))
		{
			ImGui::InputText("Engine path", command.engine_path, MAX_PATH);
			ImGui::InputText("Project name", command.project_name, MAX_STRING_OPTION_LENGTH);
			ImGui::InputText("Project path", command.project_path, MAX_PATH);

			static int platform_index = static_cast<int>(command.target_platform);
			if (ImGui::Combo("Target plaform", &platform_index, "Win64\0PS4\0XBoxOne\0Switch\0"))
			{
				switch (platform_index)
				{
					case 0: command.target_platform = TargetPlatform::Win64; break;
					case 1: command.target_platform = TargetPlatform::PS4; break;
					case 2: command.target_platform = TargetPlatform::XBoxOne; break;
					case 3: command.target_platform = TargetPlatform::Switch; break;
				}
			}
		}

		if (ImGui::CollapsingHeader("Build options", header_flags))
		{
			ImGui::Checkbox("Build", &command.should_build);

			static int configuration_index = static_cast<int>(command.build_config);
			if (ImGui::Combo("Build configuration", &configuration_index, "Debug\0Development\0Test\0Shipping\0"))
			{
				switch (configuration_index)
				{
					case 0: command.build_config = BuildConfig::Debug; break;
					case 1: command.build_config = BuildConfig::Development; break;
					case 2: command.build_config = BuildConfig::Test; break;
					case 3: command.build_config = BuildConfig::Shipping; break;
				}
			}
		}
		
		if (ImGui::CollapsingHeader("Cook options", header_flags))
		{
			ImGui::Checkbox("Cook", &command.should_cook);
		}

		if (ImGui::CollapsingHeader("Stage options", header_flags))
		{
			ImGui::Checkbox("Stage", &command.should_stage);

			if (command.should_stage)
			{
				ImGui::Checkbox("Compress assets", &command.should_compress);
				ImGui::Checkbox("Use .pak files", &command.use_pak_files);

				if (command.use_pak_files)
				{
					ImGui::Checkbox("Use multiple chunks", &command.use_mutliple_chunks);
				}
			}
		}

		if (ImGui::CollapsingHeader("Packaging options", header_flags))
		{
			ImGui::Checkbox("Package project", &command.should_package);
			ImGui::Checkbox("Build distribution version", &command.is_distribution);
		}

		if (ImGui::CollapsingHeader("Archive options", header_flags))
		{
			ImGui::Checkbox("Archive project", &command.should_archive);
			ImGui::InputText("Archive path", command.archive_path, MAX_PATH);
		}

		if (command.target_platform == TargetPlatform::PS4)
		{
			if (ImGui::CollapsingHeader("PS4", header_flags))
			{
				ImGui::InputText("TitleID", command.title_id, MAX_STRING_OPTION_LENGTH);
			}
		}

		ImGui::Separator();

		ImGuiTextBuffer log;
		log.appendf("%s/Engine/Build/BatchFiles/RunUAT.bat BuildCookRun -project=%s/%s.uproject -platform=%s -clientconfig=%s -serverconfig=%s %s -noP4 %s %s %s %s %s", 
			command.engine_path,
			command.project_path,
			command.project_name,
			TargetPlatformNames[static_cast<uint8_t>(command.target_platform)],
			BuildConfigNames[static_cast<uint8_t>(command.build_config)],
			BuildConfigNames[static_cast<uint8_t>(command.build_config)],
			OptionalTitleId(command),
			CombinedBuildOptions(command),
			CombinedCookOptions(command),
			CombinedStageOptions(command),
			CombinedPackageOptions(command),
			CombinedArchiveOptions(command)
			);

		ImGui::TextWrapped(log.begin());
		
		ImGui::Separator();

		if (ImGui::Button("Copy to clipboard", { 685.0f, 20.0f }))
		{
			ImGui::SetClipboardText(log.begin());
		}

		if (ImGui::Button("Save commandline to file", { 685.0f, 20.0f }))
		{
			SaveCommandStringToFile(log.c_str(), hwnd);
		}

		ImGui::End();

        // Rendering
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}
