#include "main.h"

// e90000000054415541564157488d68004881ec0000000048c7450000000000488958004889700048897800458bf9458be0448bea488bf98b4500894424008b75008974240044894c2400458bc8448bc2488bd1488d4d00e800000000488bc7488d4f0048f7d8481bdb4823d9488b4b00488b01488b4000ff150000000048895d008b87000000008945004533f6
// e9 00 00 00 00 54 41 55 41 56 41 57 48 8d 68 00 48 81 ec 00 00 00 00 48 c7 45 00 00 00 00 00 48 89 58 00 48 89 70 00 48 89 78 00 45 8b f9 45 8b e0 44 8b ea 48 8b f9 8b 45 00 89 44 24 00 8b 75 00 89 74 24 00 44 89 4c 24 00 45 8b c8 44 8b c2 48 8b d1 48 8d 4d 00 e8 00 00 00 00 48 8b c7 48 8d 4f 00 48 f7 d8 48 1b db 48 23 d9 48 8b 4b 00 48 8b 01 48 8b 40 00 ff 15 00 00 00 00 48 89 5d 00 8b 87 00 00 00 00 89 45 00 45 33 f6
// e9 ? ? ? ? 54 41 55 41 56 41 57 48 8d 68 ? 48 81 ec ? ? ? ? 48 c7 45 ? ? ? ? ? 48 89 58 ? 48 89 70 ? 48 89 78 ? 45 8b f9 45 8b e0 44 8b ea 48 8b f9 8b 45 ? 89 44 24 ? 8b 75 ? 89 74 24 ? 44 89 4c 24 ? 45 8b c8 44 8b c2 48 8b d1 48 8d 4d ? e8 ? ? ? ? 48 8b c7 48 8d 4f ? 48 f7 d8 48 1b db 48 23 d9 48 8b 4b ? 48 8b 01 48 8b 40 ? ff 15 ? ? ? ? 48 89 5d ? 8b 87 ? ? ? ? 89 45 ? 45 33 f6

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		g_hInstance = hModule;
		if (init())
		{
			tslog::log("Successful Initialization!\n");
			CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(run), nullptr, 0, nullptr);
		}
	}
	return true;
}

bool init()
{
	tslog::init(LOG_LEVEL_TARGET, SHOULD_ALLOC_CONSOLE);
	tslog::verbose("Starting main initialization..");

	if (!getBaseModule())
	{
		tslog::critical("Module Base Address not found.");
		end();
		return false;
	}
	else
	{
		tslog::log("Module Base Address: %p", (void*)g_BaseModule.lpBaseOfDll);
	}

	tslog::verbose("Hooking Present function..");
	getModule();
	hookPresent();
	if (dxgi == NULL)
	{
		tslog::critical("dxgi.dll not found.");
		end();
		return false;
	}
	if (!oDX11Present)
	{
		tslog::critical("DX11 Present function not found.");
		end();
		return false;
	}
	hookResizeBuffer();
	if (!oDX11ResizeBuffer)
	{
		tslog::critical("DX11 ResizeBuffers function not found.");
		end();
		return false;
	}
	uintptr_t world = findGWorld();
	if (!world)
	{
		tslog::critical("GWorld not found.");
		end();
		return false;
	}
	uintptr_t objects = findGObjects();
	if (!objects)
	{
		tslog::critical("GObjects not found.");
		end();
		return false;
	}
	uintptr_t names = findGNames();
	if (!names)
	{
		tslog::critical("GNames not found.");
		end();
		return false;
	}
	if (!initUE4(world, objects, names))
	{
		tslog::critical("UE4 vars can not be initialized.");
		end();
		return false;
	}

	oSetCursor = (fnSetCursor)hook(SetCursor, hkSetCursor);
	oSetCursorPos = (fnSetCursorPos)hook(SetCursorPos, hkSetCursorPos);

	g_Running = true;
	return true;
}

bool end()
{
	tslog::log("Shutting down..");
	tslog::log("Unhooking Present function..");
	if (oDX11Present)
		unhook((void*)oDX11Present);
	if (oDX11ResizeBuffer)
		unhook((void*)oDX11ResizeBuffer);
	if (oWindowProc)
		SetWindowLongPtr(g_TargetWindow, GWLP_WNDPROC, (LONG_PTR)oWindowProc);
	if (oSetCursor)
		unhook((void*)oSetCursor);
	if (oSetCursorPos)
		unhook((void*)oSetCursorPos);
	EngineShutdown();
	clearDX11Objects();
	tslog::shutdown();
	FreeLibraryAndExitThread(g_hInstance, 0);
	return true;
}

void run()
{
	tslog::verbose("Starting main loop..\n");
	while (g_Running)
	{
		if (GetAsyncKeyState(VK_INSERT) & 1)
		{
			g_ShowMenu = !g_ShowMenu;
		}
		if (GetAsyncKeyState(VK_END) & 1)
		{
			g_Running = false;
		}
		Sleep(20);
	}
	end();
}

HRESULT presentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
	if (!g_Running) return oDX11Present(swapChain, syncInterval, flags);

	if (!device)
	{
		ID3D11Texture2D* buffer;

		swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer));
		swapChain->GetDevice(IID_PPV_ARGS(&device));
		device->CreateRenderTargetView(buffer, nullptr, &rtv);
		device->GetImmediateContext(&context);

		safe_release(buffer);

		DXGI_SWAP_CHAIN_DESC desc;
		swapChain->GetDesc(&desc);
		auto& window = desc.OutputWindow;
		g_TargetWindow = window;

		if (oWindowProc && g_TargetWindow)
		{
			SetWindowLongPtrA(g_TargetWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWindowProc));
			oWindowProc = nullptr;
		}
		if (g_TargetWindow) {
			oWindowProc = (WNDPROC)SetWindowLongPtr(g_TargetWindow, GWLP_WNDPROC, (LONG_PTR)windowProcHookEx);
			tslog::log("Original WndProc function: %p\n", oWindowProc);
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 };
		ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Georgia.ttf", 17);
		ImFontConfig config;
		config.MergeMode = true;
		io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_data, font_awesome_size, 19.0f, &config, icons_ranges);
		io.Fonts->Build();

		ImGui_ImplWin32_Init(g_TargetWindow);
		ImGui_ImplDX11_Init(device, context);
		ImGui_ImplDX11_CreateDeviceObjects();
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();


	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::Begin("overlay", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);
	auto& io = ImGui::GetIO();
	ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);

	auto drawList = ImGui::GetCurrentWindow()->DrawList;

	if (updateGameVars()) // 2k22, PC can hadle it
	{
		render(ImGui::GetCurrentWindow()->DrawList);
	}

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	ImGuiStyle* style = &ImGui::GetStyle();

	style->WindowPadding = ImVec2(15, 15);
	style->WindowRounding = 1.0f;
	style->FramePadding = ImVec2(5, 5);
	style->FrameRounding = 4.0f;
	style->ItemSpacing = ImVec2(12, 8);
	style->ItemInnerSpacing = ImVec2(8, 6);
	style->IndentSpacing = 25.0f;
	style->ScrollbarSize = 15.0f;
	style->ScrollbarRounding = 9.0f;
	style->GrabMinSize = 5.0f;
	style->GrabRounding = 3.0f;

	style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
	style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
	style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
	style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
	style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
	style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);


	if (g_ShowMenu)
	{
		ImGui::SetNextWindowSize(ImVec2(1280, 500), ImGuiCond_Once);
		ImGui::Begin("SOT Internal (Flag#1337)", 0, ImGuiWindowFlags_NoCollapse);


		if (ImGui::Button("Save Settings"))
		{
			do {
				wchar_t buf[MAX_PATH];
				GetModuleFileNameW(g_hInstance, buf, MAX_PATH);
				fs::path path = fs::path(buf).remove_filename() / ".settings";
				auto file = CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (file == INVALID_HANDLE_VALUE) break;
				DWORD written;
				if (WriteFile(file, &Config::cfg, sizeof(Config::cfg), &written, 0)) ImGui::OpenPopup("##SettingsSaved");
				CloseHandle(file);
			} while (false);
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Settings"))
		{
			do {
				wchar_t buf[MAX_PATH];
				GetModuleFileNameW(g_hInstance, buf, MAX_PATH);
				fs::path path = fs::path(buf).remove_filename() / ".settings";
				auto file = CreateFileW(path.wstring().c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (file == INVALID_HANDLE_VALUE) break;
				DWORD readed;
				if (ReadFile(file, &Config::cfg, sizeof(Config::cfg), &readed, 0))  ImGui::OpenPopup("##SettingsLoaded");
				CloseHandle(file);
			} while (false);
		}

		ImGui::Separator();

		if (ImGui::BeginTabBar("Bars"))
		{


			if (ImGui::BeginTabItem("Client"))
			{
				ImGui::Text("Global Client");

				ImGui::Checkbox("Main Switch (Client)", &Config::cfg.client.enable);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				const char* crosshair[] = { "None", "Circle", "Cross" };
				ImGui::Checkbox("Change FOV", &Config::cfg.client.fovEnable);
				ImGui::Checkbox("Enable Custom Time of Day (Experimental | Not safe)", &Config::cfg.client.bCustomTOD);
				ImGui::Checkbox("Change Spyglass FOV", &Config::cfg.client.spyRClickMode);
				ImGui::Checkbox("Crosshair", &Config::cfg.client.crosshair);
				ImGui::Checkbox("Show Oxygen Level", &Config::cfg.client.oxygen);

				ImGui::Spacing();

				ImGui::SliderFloat("FOV Value", &Config::cfg.client.fov, 60.f, 150.f, "%.0f");
				ImGui::SliderFloat("Spyglass FOV Value", &Config::cfg.client.spyglassFovMul, 1.f, 25.f, "%.0f");
				ImGui::SliderFloat("Time of Day", &Config::cfg.client.customTOD, 1.f, 24.f, "%.0f:00");
				ImGui::SliderFloat("Eye of Reach FOV Factor", &Config::cfg.client.sniperFovMul, 1.f, 25.f, "%.0f");
				ImGui::SliderFloat("Crosshair Size", &Config::cfg.client.crosshairSize, 1.f, 50.f, "%.0f");
				ImGui::SliderFloat("Crosshair Thickness", &Config::cfg.client.crosshairThickness, 1.f, 50.f, "%.0f");

				ImGui::Spacing();

				ImGui::Combo("Crosshair Type", reinterpret_cast<int*>(&Config::cfg.client.crosshairType), crosshair, IM_ARRAYSIZE(crosshair));
				ImGui::ColorEdit4("Crosshair Color", &Config::cfg.client.crosshairColor.x, 0);



				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Actor ESP"))
			{

				ImGui::Text("Global");
				ImGui::Checkbox("Main Switch (Actor)", &Config::cfg.esp.enable);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Text("Players");
				ImGui::Checkbox("Enable Player ESP", &Config::cfg.esp.players.enable);
				ImGui::Checkbox("Snaplines", &Config::cfg.esp.players.tracers);
				ImGui::Checkbox("Teammates", &Config::cfg.esp.players.team);
				ImGui::Checkbox("Snaplines", &Config::cfg.esp.players.tracers);

				ImGui::Spacing();

				ImGui::SliderFloat("Snapline Thickness", &Config::cfg.esp.players.tracersThickness, 1.f, 10.f, "%.0f");
				ImGui::SliderFloat("Player Distance", &Config::cfg.esp.players.renderDistance, 1.f, 2000.f, "%.0f");

				ImGui::Spacing();

				ImGui::ColorEdit4("Visible Color", &Config::cfg.esp.players.colorVisible.x, 0);
				ImGui::ColorEdit4("Invisible Color", &Config::cfg.esp.players.colorInvisible.x, 0);


				ImGui::Spacing();


				ImGui::Text("Mobs");

				ImGui::Checkbox("Enable Mob ESP", &Config::cfg.esp.skeletons.enable);
				ImGui::SliderFloat("Mob Distance", &Config::cfg.esp.skeletons.renderDistance, 1.f, 500.f, "%.0f");
				ImGui::ColorEdit4("Color", &Config::cfg.esp.skeletons.color.x, 0);


				ImGui::Spacing();


				ImGui::Text("Ships");

				ImGui::Checkbox("Enable Ship ESP", &Config::cfg.esp.ships.enable);
				ImGui::Checkbox("Show Holes", &Config::cfg.esp.ships.holes);
				ImGui::Checkbox("Skeleton", &Config::cfg.esp.ships.skeletons);
				ImGui::Checkbox("Ghost Ships", &Config::cfg.esp.ships.ghosts);
				ImGui::Checkbox("Show Ladders Position", &Config::cfg.esp.ships.showLadders);
				ImGui::Checkbox("Ship Trajectory", &Config::cfg.esp.ships.shipTray);

				ImGui::Spacing();

				ImGui::SliderFloat("Trajectory Thickness", &Config::cfg.esp.ships.shipTrayThickness, 0.f, 1000.f, "%.0f");
				ImGui::SliderFloat("Trajectory Height", &Config::cfg.esp.ships.shipTrayHeight, -10.f, 20.f, "%.0f");
				ImGui::SliderFloat("Ship ESP Distance", &Config::cfg.esp.ships.renderDistance, 1.f, 5000.f, "%.0f");

				ImGui::Spacing();

				ImGui::ColorEdit4("Trajectory Color", &Config::cfg.esp.ships.shipTrayCol.x, 0);
				ImGui::ColorEdit4("Ship Color", &Config::cfg.esp.ships.color.x, 0);

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Second Hand Actor ESP"))
			{
				ImGui::Text("Global");
				ImGui::Checkbox("Main Switch (2nd Actors)", &Config::cfg.esp.enable);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Text("Island");

				ImGui::Checkbox("Enable Island ESP", &Config::cfg.esp.islands.enable);
				ImGui::Checkbox("Map Marks", &Config::cfg.esp.islands.marks);
				ImGui::Checkbox("Show Rare Spots", &Config::cfg.esp.islands.rareNames);
				ImGui::Checkbox("Vaults", &Config::cfg.esp.islands.vaults);
				ImGui::Checkbox("Barrels", &Config::cfg.esp.islands.barrels);
				ImGui::Checkbox("Peek Barrels", &Config::cfg.esp.islands.barrelspeek);
				ImGui::Checkbox("Peek (R, Toggle)", &Config::cfg.esp.islands.barrelstoggle);
				ImGui::Checkbox("Ammo Chests", &Config::cfg.esp.islands.ammoChest);

				ImGui::Spacing();

				ImGui::Text("Filter Rare Items");
				ImGui::InputText("Items", Config::cfg.esp.islands.rareNamesFilter, IM_ARRAYSIZE(Config::cfg.esp.islands.rareNamesFilter));

				ImGui::Spacing();

				ImGui::SliderFloat("Barrels Distance", &Config::cfg.esp.islands.barrelsRenderDistance, 1.f, 10000.f, "%.0f");
				ImGui::SliderFloat("Ammo Chests Distance", &Config::cfg.esp.islands.ammoChestRenderDistance, 1.f, 10000.f, "%.0f");
				ImGui::SliderFloat("Vaults Distance", &Config::cfg.esp.islands.vaultsRenderDistance, 1.f, 10000.f, "%.0f");
				ImGui::SliderFloat("Rare Spots Distance", &Config::cfg.esp.islands.decalsRenderDistance, 1.f, 1000.f, "%.0f");
				ImGui::SliderFloat("Map Marks Distance", &Config::cfg.esp.islands.marksRenderDistance, 1.f, 10000.f, "%.0f");
				ImGui::SliderFloat("Size", &Config::cfg.esp.islands.size, 1.f, 10.f, "%.0f");
				ImGui::SliderFloat("Island Distance", &Config::cfg.esp.islands.renderDistance, 1.f, 10000.f, "%.0f");

				ImGui::Spacing();

				ImGui::ColorEdit4("Ammo Chests Color", &Config::cfg.esp.islands.ammoChestColor.x, 0);
				ImGui::ColorEdit4("Island Color", &Config::cfg.esp.islands.color.x, 0);
				ImGui::ColorEdit4("Barrels Color", &Config::cfg.esp.islands.barrelsColor.x, 0);
				ImGui::ColorEdit4("Vaults Color", &Config::cfg.esp.islands.vaultsColor.x, 0);
				ImGui::ColorEdit4("Rare Spots Color", &Config::cfg.esp.islands.decalsColor.x, 0);
				ImGui::ColorEdit4("Map Marks Color", &Config::cfg.esp.islands.marksColor.x, 0);

				ImGui::Spacing();

				ImGui::Text("Items");

				ImGui::Checkbox("Enable Item ESP", &Config::cfg.esp.items.enable);
				ImGui::Checkbox("Toggle Names (R)", &Config::cfg.esp.items.nameToggle);
				ImGui::Checkbox("Animals", &Config::cfg.esp.items.animals);
				ImGui::Checkbox("Lost Cargo Assist", &Config::cfg.esp.items.lostCargo);
				ImGui::Checkbox("GhostShips Rewards", &Config::cfg.esp.items.gsRewards);

				ImGui::Spacing();

				ImGui::ColorEdit4("Animals Color", &Config::cfg.esp.items.animalsColor.x, 0);
				ImGui::ColorEdit4("Clues Color", &Config::cfg.esp.items.cluesColor.x, 0);
				ImGui::ColorEdit4("GhostShips Reward Color", &Config::cfg.esp.items.gsRewardsColor.x, 0);
				ImGui::ColorEdit4("Item Color", &Config::cfg.esp.items.color.x, 0);

				ImGui::Spacing();

				ImGui::SliderFloat("Item Distance", &Config::cfg.esp.items.renderDistance, 1.f, 500.f, "%.0f");
				ImGui::SliderFloat("Animals Distance", &Config::cfg.esp.items.animalsRenderDistance, 1.f, 500.f, "%.0f");

				ImGui::Spacing();

				ImGui::Text("Others");

				ImGui::Checkbox("Enable Other ESP", &Config::cfg.esp.others.enable);
				ImGui::Checkbox("Shipwrecks", &Config::cfg.esp.others.shipwrecks);
				ImGui::Checkbox("World Events", &Config::cfg.esp.others.events);
				ImGui::Checkbox("Mermaids", &Config::cfg.esp.others.mermaids);
				ImGui::Checkbox("Rowboats", &Config::cfg.esp.others.rowboats);
				ImGui::Checkbox("Sharks", &Config::cfg.esp.others.sharks);

				ImGui::Spacing();

				ImGui::SliderFloat("Sharks Distance", &Config::cfg.esp.others.sharksRenderDistance, 1.f, 500.f, "%.0f");
				ImGui::SliderFloat("World Distance", &Config::cfg.esp.others.eventsRenderDistance, 1.f, 10000.f, "%.0f");
				ImGui::SliderFloat("Shipwrecks Distance", &Config::cfg.esp.others.shipwrecksRenderDistance, 1.f, 5000.f, "%.0f");
				ImGui::SliderFloat("Mermaids Distance", &Config::cfg.esp.others.mermaidsRenderDistance, 1.f, 1000.f, "%.0f");
				ImGui::SliderFloat("Rowboats Distance", &Config::cfg.esp.others.rowboatsRenderDistance, 1.f, 3500.f, "%.0f");

				ImGui::Spacing();

				ImGui::ColorEdit4("Rowboats Color", &Config::cfg.esp.others.rowboatsColor.x, 0);
				ImGui::ColorEdit4("Sharks Color", &Config::cfg.esp.others.sharksColor.x, 0);
				ImGui::ColorEdit4("World Color", &Config::cfg.esp.others.eventsColor.x, 0);
				ImGui::ColorEdit4("Shipwrecks Color", &Config::cfg.esp.others.shipwrecksColor.x, 0);
				ImGui::ColorEdit4("Mermaids Color", &Config::cfg.esp.others.mermaidsColor.x, 0);



				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Aimbot"))
			{
				ImGui::Text("Global");
				ImGui::Checkbox("Main switch (Aimbot)", &Config::cfg.aim.enable);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Text("Weapon");
				ImGui::Checkbox("Aimbot (W)", &Config::cfg.aim.weapon.enable);
				ImGui::Checkbox("Players (W)", &Config::cfg.aim.weapon.players);
				ImGui::Checkbox("Skeletons (W)", &Config::cfg.aim.weapon.skeletons);
				ImGui::Checkbox("Gunpowder", &Config::cfg.aim.weapon.kegs);
				ImGui::Checkbox("Instant Shot", &Config::cfg.aim.weapon.trigger);
				ImGui::Checkbox("Visible Only", &Config::cfg.aim.weapon.visibleOnly);

				ImGui::Spacing();

				ImGui::SliderFloat("Pitch (W)", &Config::cfg.aim.weapon.fPitch, 1.f, 100.f, "%.0f");
				ImGui::SliderFloat("Yaw (W)", &Config::cfg.aim.weapon.fYaw, 1.f, 100.f, "%.0f");
				ImGui::SliderFloat("Smoothness", &Config::cfg.aim.weapon.smooth, 1.f, 10.f, "%.0f");
				ImGui::SliderFloat("Height", &Config::cfg.aim.weapon.height, 1.f, 100.f, "%.0f");

				ImGui::Spacing();

				ImGui::Text("Cannon");
				ImGui::Checkbox("Aimbot (C)", &Config::cfg.aim.cannon.enable);
				ImGui::Checkbox("Draw Trajectory", &Config::cfg.aim.cannon.drawPred);
				ImGui::Checkbox("Instant Shot", &Config::cfg.aim.cannon.instant);
				ImGui::Checkbox("Chain Shots", &Config::cfg.aim.cannon.chains);
				ImGui::Checkbox("Players (W)", &Config::cfg.aim.cannon.players);
				ImGui::Checkbox("Skeletons (W)", &Config::cfg.aim.cannon.skeletons);
				ImGui::Checkbox("Ghost Ships", &Config::cfg.aim.cannon.ghostShips);
				ImGui::Checkbox("Aim To Hull", &Config::cfg.aim.cannon.lowAim);
				ImGui::Checkbox("Player to Deck", &Config::cfg.aim.cannon.deckshots);
				ImGui::Checkbox("Visible Only", &Config::cfg.aim.cannon.visibleOnly);
				ImGui::Checkbox("Improved Cannon Aim", &Config::cfg.aim.cannon.improvedVersion);

				ImGui::Spacing();

				ImGui::SliderFloat("Pitch (C)", &Config::cfg.aim.cannon.fPitch, 1.f, 100.f, "%.0f");
				ImGui::SliderFloat("Yaw (C)", &Config::cfg.aim.cannon.fYaw, 1.f, 100.f, "%.0f");


				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Game"))
			{
				ImGui::Text("Global Game");
				ImGui::Checkbox("Master Switch (Game)", &Config::cfg.game.enable);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Checkbox("Current Ship Info", &Config::cfg.game.shipInfo);
				ImGui::Checkbox("Map Pins", &Config::cfg.game.mapPins);
				ImGui::Checkbox("Player List", &Config::cfg.game.playerList);
				ImGui::Checkbox("Cooking Tracker", &Config::cfg.game.cooking);
				ImGui::Checkbox("Anti Idle", &Config::cfg.game.noIdleKick);
				ImGui::Checkbox("TP to Floor of Ocean (C)", &Config::cfg.game.walkUnderwater);
				ImGui::Checkbox("Show Sunken Location", &Config::cfg.game.showSunk);

				if (ImGui::Button("Custom Pirate"))
				{
					if (loadPirateGenerator())
					{
						engine::bInPirateGenerator = true;
						g_ShowMenu = false;
					}
					else
					{
						ImGui::OpenPopup("##PirateGeneratorLoaded");
					}
				}
				if (ImGui::BeginPopupModal("##PirateGeneratorLoaded", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
				{
					ImGui::Text("\nSomething went wrong.\nThis must be used in the character creator.\n\n");
					ImGui::Separator();
					if (ImGui::Button("OK", { 170.f , 0.f })) { ImGui::CloseCurrentPopup(); }
					ImGui::EndPopup();
				}

				ImGui::Spacing();

				ImGui::ColorEdit4("Sunken Ship Color", &Config::cfg.game.sunkColor.x, 0);

				ImGui::Spacing();

				if (ImGui::Button("Clear Sunken List"))
				{
					ClearSunkList();
				}
				ImGui::EndTabItem();
			}



			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	if (engine::bInPirateGenerator)
	{
		PirateGeneratorLineUpUI* p = (PirateGeneratorLineUpUI*)getPirateGenerator();
		auto pirateDescs = p->CarouselPirateDescs;
		ImGui::SetNextWindowPos(ImVec2(10, 25), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
		ImGui::Begin("Pirate Customizer", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		ImGui::Text("Open Cheat Menu (INSERT) to be able to interact with this Window");
		if (ImGui::Button("Close"))
		{
			engine::bInPirateGenerator = false;
		}

		ImGui::Separator();
		ImGui::Text("Template Pirate Seed: %d", tPirateSeedN);
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			ZeroMemory(tPirateSeed, sizeof(tPirateSeed));
			tPirateSeedN = 0;
		}
		ImGui::InputText("Template Pirate Seed", tPirateSeed, IM_ARRAYSIZE(tPirateSeed));
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("This will affect the Characters Face! This also may look different in-game");
		}
		ImGui::SliderInt("Template Pirate Gender", &tPirateGender, -1, 2);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Syntax 1 = Male, Syntax 2 = Female");
		}
		ImGui::SliderFloat("Template Pirate Age", &tPirateAge, -1.0f, 1.0f, "%.1f");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("");
		}
		ImGui::SliderInt("Template Pirate Ethnicity", &tPirateEthnicity, -1, 4);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Syntax 1 = Asian , Syntax 2 = Black, Syntax 3 = White ");
		}
		ImGui::SliderFloat("Template Pirate BodyType", &tPirateBodyType, -1.0f, 1.0f, "%.1f");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Changes the Characters Body type");
		}
		ImGui::SliderFloat("Template Pirate BodyType Modifier", &tPirateBodyTypeModifier, -1.0f, 1.0f, "%.1f");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Changes the Characters Body type V2 (use if other option doesnt work)");
		}
		ImGui::SliderFloat("Template Pirate Dirtiness", &tPirateDirtiness, -1.0f, 1.0f, "%.2f");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Makes Character Dirtier");
		}
		ImGui::SliderFloat("Template Pirate Wonkiness", &tPirateWonkiness, -1.0f, 1.0f, "%.2f");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Zaza Simulator");
		}
		for (UINT8 i = 0; i < pirateDescs.Count; i++)
		{
			char pBuf[0x64];
			int filterSize = sprintf_s(pBuf, sizeof(pBuf), tPirateSeed);
			if (filterSize != 0)
			{
				try
				{
					tPirateSeedN = std::stoi(std::string(pBuf));
				}
				catch (...)
				{
					tPirateSeedN = 0;
					tslog::info("Seed must be a valid int32 number.");
				}
			}
			else
			{
				tPirateSeedN = 0;
			}
			if (tPirateSeedN != 0) pirateDescs[i].Seed = tPirateSeedN;
			if (tPirateGender > 0) pirateDescs[i].Gender = tPirateGender;
			if (tPirateAge >= 0.0f) pirateDescs[i].Age = tPirateAge;
			if (tPirateEthnicity > 0) pirateDescs[i].Ethnicity = tPirateEthnicity;

			if (tPirateBodyType >= 0.0f) pirateDescs[i].BodyShape.NormalizedAngle = tPirateBodyType;
			if (tPirateBodyTypeModifier >= 0.0f) pirateDescs[i].BodyShape.RadialDistance = tPirateBodyTypeModifier;
			if (tPirateDirtiness >= 0.0f) pirateDescs[i].Dirtiness = tPirateDirtiness;
			if (tPirateWonkiness >= 0.0f) pirateDescs[i].Wonkiness = tPirateWonkiness;
		}
		ImGui::End();
	}

	ImGui::Render();
	context->OMSetRenderTargets(1, &rtv, nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	return oDX11Present(swapChain, syncInterval, flags);
}

void clearDX11Objects()
{
	safe_release(rtv);
	safe_release(context);
	safe_release(device);
	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();
}

LRESULT CALLBACK windowProcHookEx(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (g_ShowMenu)
	{
		if (ImGui::GetCurrentContext() != NULL)
		{
			ImGuiIO& io = ImGui::GetIO();
			switch (msg)
			{
			case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
			case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
			{
				int button = 0;
				if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
				if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
				if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
				if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				if (!ImGui::IsAnyMouseDown() && GetCapture() == NULL)
					SetCapture(hWnd);
				io.MouseDown[button] = true;
				break;
			}
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
			case WM_XBUTTONUP:
			{
				int button = 0;
				if (msg == WM_LBUTTONUP) { button = 0; }
				if (msg == WM_RBUTTONUP) { button = 1; }
				if (msg == WM_MBUTTONUP) { button = 2; }
				if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				io.MouseDown[button] = false;
				if (!ImGui::IsAnyMouseDown() && GetCapture() == hWnd)
					ReleaseCapture();
				break;
			}
			case WM_MOUSEWHEEL:
				io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
				break;
			case WM_MOUSEHWHEEL:
				io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
				break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				if (wParam < 256)
					io.KeysDown[wParam] = 1;
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				if (wParam < 256)
					io.KeysDown[wParam] = 0;
				break;
			case WM_CHAR:
				if (wParam > 0 && wParam < 0x10000)
					io.AddInputCharacterUTF16((unsigned short)wParam);
				break;
			}

		}
		ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
		LPTSTR win32_cursor = IDC_ARROW;
		switch (imgui_cursor)
		{
		case ImGuiMouseCursor_Arrow:        win32_cursor = IDC_ARROW; break;
		case ImGuiMouseCursor_TextInput:    win32_cursor = IDC_IBEAM; break;
		case ImGuiMouseCursor_ResizeAll:    win32_cursor = IDC_SIZEALL; break;
		case ImGuiMouseCursor_ResizeEW:     win32_cursor = IDC_SIZEWE; break;
		case ImGuiMouseCursor_ResizeNS:     win32_cursor = IDC_SIZENS; break;
		case ImGuiMouseCursor_ResizeNESW:   win32_cursor = IDC_SIZENESW; break;
		case ImGuiMouseCursor_ResizeNWSE:   win32_cursor = IDC_SIZENWSE; break;
		case ImGuiMouseCursor_Hand:         win32_cursor = IDC_HAND; break;
		case ImGuiMouseCursor_NotAllowed:   win32_cursor = IDC_NO; break;
		}
		oSetCursor(::LoadCursor(NULL, win32_cursor));
		if (msg == WM_KEYUP) return CallWindowProc(oWindowProc, hWnd, msg, wParam, lParam);
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return CallWindowProc(oWindowProc, hWnd, msg, wParam, lParam);
}

HRESULT resizeBufferHook(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
	clearDX11Objects();
	return oDX11ResizeBuffer(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
}

bool getBaseModule()
{
	return K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(nullptr), &g_BaseModule, sizeof(MODULEINFO));
}

void getModule()
{
	mSize = 0;
	dxgi = (uintptr_t)ts::GetMBA("dxgi.dll", mSize);
	if (dxgi != NULL)
		tslog::log("dxgi.dll Address: %p", (void*)dxgi);
}
void hookPresent()
{
	if (dxgi == NULL) return;
	char Sign[] = "\xe9\x00\x00\x00\x00\x48\x89\x74\x24\x00\x55";
	char Mask[] = "x????xxxx?x";
	uintptr_t Addr = ts::Aobs(Sign, Mask, dxgi, (uintptr_t)&mSize);
	oDX11Present = (DX11Present)hook((void*)((uintptr_t)Addr), presentHook);
	tslog::log("Original Present function: %p", oDX11Present);
}

void hookResizeBuffer()
{
	if (dxgi == NULL) return;
	char Sign[] = "\xe9\x00\x00\x00\x00\x54\x41\x55\x41\x56\x41\x57\x48\x8d\x68\x00\x48\x81\xec\x00\x00\x00\x00\x48\xc7\x45\x00\x00\x00\x00\x00\x48\x89\x58\x00\x48\x89\x70\x00\x48\x89\x78\x00\x45\x8b\xf9\x45\x8b\xe0\x44\x8b\xea\x48\x8b\xf9\x8b\x45\x00\x89\x44\x24\x00\x8b\x75\x00\x89\x74\x24\x00\x44\x89\x4c\x24\x00\x45\x8b\xc8\x44\x8b\xc2\x48\x8b\xd1\x48\x8d\x4d\x00\xe8\x00\x00\x00\x00\x48\x8b\xc7\x48\x8d\x4f\x00\x48\xf7\xd8\x48\x1b\xdb\x48\x23\xd9\x48\x8b\x4b\x00\x48\x8b\x01\x48\x8b\x40\x00\xff\x15\x00\x00\x00\x00\x48\x89\x5d\x00\x8b\x87\x00\x00\x00\x00\x89\x45\x00\x45\x33\xf6";
	char Mask[] = "x????xxxxxxxxxx?xxx????xxx?????xxx?xxx?xxx?xxxxxxxxxxxxxx?xxx?xx?xxx?xxxx?xxxxxxxxxxxx?x????xxxxxx?xxxxxxxxxxxx?xxxxxx?xx????xxx?xx????xx?xxx";
	uintptr_t Addr = ts::Aobs(Sign, Mask, dxgi, (uintptr_t)&mSize);
	oDX11ResizeBuffer = (DX11ResizeBuffer)hook((void*)((uintptr_t)Addr), resizeBufferHook);
	tslog::log("Original ResizeBuffers function: %p", oDX11ResizeBuffer);
}

uintptr_t findGWorld()
{
	if (dxgi == NULL) return 0;
	char Sign[] = "\x48\x8B\x05\x00\x00\x00\x00\x48\x8B\x88\x00\x00\x00\x00\x48\x85\xC9\x74\x06\x48\x8B\x49\x70";
	char Mask[] = "xxx????xxx????xxxxxxxxx";
	uintptr_t Addr = ts::Aobs(Sign, Mask, (uintptr_t)g_BaseModule.lpBaseOfDll, (uintptr_t)g_BaseModule.lpBaseOfDll + (uintptr_t)g_BaseModule.SizeOfImage);
	if (!Addr) return 0;
	auto offset = *reinterpret_cast<uint32_t*>(Addr + 3);
	Addr = Addr + 7 + offset;
	tslog::log("GWorld Address: %p", Addr);
	return Addr;
}
uintptr_t findGObjects()
{
	if (dxgi == NULL) return 0;
	char Sign[] = "\x89\x0D\x00\x00\x00\x00\x48\x8B\xDF\x48\x89\x5C\x24";
	char Mask[] = "xx????xxxxxxx";
	uintptr_t Addr = ts::Aobs(Sign, Mask, (uintptr_t)g_BaseModule.lpBaseOfDll, (uintptr_t)g_BaseModule.lpBaseOfDll + (uintptr_t)g_BaseModule.SizeOfImage);
	if (!Addr) return 0;
	auto offset = *reinterpret_cast<uint32_t*>(Addr + 2);
	Addr = Addr + 6 + offset + 16;
	tslog::log("GObjects Address: %p", Addr);
	return Addr;
}
uintptr_t findGNames()
{
	if (dxgi == NULL) return 0;
	char Sign[] = "\x48\x8B\x1D\x00\x00\x00\x00\x48\x85\xDB\x75\x00\xB9\x08\x04\x00\x00";
	char Mask[] = "xxx????xxxx?xxxxx";
	uintptr_t Addr = ts::Aobs(Sign, Mask, (uintptr_t)g_BaseModule.lpBaseOfDll, (uintptr_t)g_BaseModule.lpBaseOfDll + (uintptr_t)g_BaseModule.SizeOfImage);
	if (!Addr) return 0;
	auto offset = *reinterpret_cast<uint32_t*>(Addr + 3);
	Addr = Addr + 7 + offset;
	tslog::log("GNames Address: %p", Addr);
	return Addr;
}

HCURSOR WINAPI hkSetCursor(HCURSOR hCursor)
{
	if (g_ShowMenu)
	{
		return NULL;
	}
	return oSetCursor(hCursor);
}

BOOL WINAPI hkSetCursorPos(int x, int y)
{
	if (g_ShowMenu)
	{
		return false;
	}
	return oSetCursorPos(x, y);
}
