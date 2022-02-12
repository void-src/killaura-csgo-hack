#define NOMINMAX
#include <Windows.h>
#include "../config/config.hpp"
#include "../../sdk/sdk.hpp"
#include "../../helpers/utils.hpp"
#include "../../helpers/input.hpp"
#include "../hooks/hooks.hpp"
#include "../menu/menu.hpp"
#include "../config/options.hpp"
#include "../render/render.hpp"

#pragma comment(lib, "Winmm.lib")

DWORD WINAPI OnDllAttach( LPVOID base ) {

	if (Utils::WaitForModules(10000, { L"client.dll", L"engine.dll", L"shaderapidx9.dll" }) == WAIT_TIMEOUT) return FALSE;
	
#ifdef _DEBUG
	Utils::AttachConsole();
#endif

	try {
		CConfig::Get().Setup();
		Utils::ConsolePrint("Initializing...\n");

		Interfaces::Initialize();

		NetvarSys::Get().Initialize();
		InputSys::Get().Initialize();
		Render::Get().Initialize();
		Menu::Get().Initialize();

		Hooks::Initialize();

		InputSys::Get().RegisterHotkey(VK_INSERT, [base]() {
			Menu::Get().Toggle();
		});

		if (k_skins.size() == 0) initialize_kits();

		Utils::ConsolePrint("Finished.\n");

		while (!g_Unload) Sleep(1000);

		FreeLibraryAndExitThread(static_cast<HMODULE>(base), 1);
	}
	catch (const std::exception& ex) {
		Utils::ConsolePrint("An error occured during initialization:\n");
		Utils::ConsolePrint("%s\n", ex.what());
		Utils::ConsolePrint("Press any key to exit.\n");
		Utils::ConsoleReadKey();
		Utils::DetachConsole();

		FreeLibraryAndExitThread(static_cast<HMODULE>(base), 1);
	}
}

BOOL WINAPI OnDllDetach() {
#ifdef _DEBUG
	Utils::DetachConsole();
#endif
	Hooks::Shutdown();

	Menu::Get().Shutdown();
	return TRUE;
}

BOOL WINAPI DllMain(
	_In_      HINSTANCE hinstDll,
	_In_      DWORD     fdwReason,
	_In_opt_  LPVOID    lpvReserved
) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hinstDll);
		CreateThread(nullptr, 0, OnDllAttach, hinstDll, 0, nullptr);
		return TRUE;
	case DLL_PROCESS_DETACH:
		if (lpvReserved == nullptr)
			return OnDllDetach();
		return TRUE;
	default:
		return TRUE;
	}
}
