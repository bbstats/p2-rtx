#include "std_include.hpp"
#include <Psapi.h>
#include <wincrypt.h>
#include "components/common/flags.hpp"
#include "components/common/crash_handler.hpp"

std::unordered_set<HWND> wnd_class_list;

namespace p2
{
	BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lParam)
	{
		DWORD window_pid, target_pid = static_cast<DWORD>(lParam);
		GetWindowThreadProcessId(hwnd, &window_pid);

		if (window_pid == target_pid && IsWindowVisible(hwnd))
		{
			char class_name[256];
			GetClassNameA(hwnd, class_name, sizeof(class_name));

			if (!wnd_class_list.contains(hwnd))
			{
				char debug_msg[256];
				wsprintfA(debug_msg, "> HWND: %p, PID: %u, Class: %s, Visible: %d \n", hwnd, window_pid, class_name, IsWindowVisible(hwnd));
				common::log("Main", debug_msg, common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
				wnd_class_list.insert(hwnd);
			}

			if (std::string_view(class_name).contains("Valve001"s))
			{
				glob::main_window = hwnd;
				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL get_module_handle_and_size(utils::mem::module_info& module_info, LPCSTR name, uint32_t& timeout)
	{
		while (!module_info.handle)
		{
			HMODULE module = GetModuleHandleA(name);
			if (module)
			{
				MODULEINFO info{};
				if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
				{
					common::log("Main", std::format("Failed to get module information for {}. Error: (0x{:X})", name ? name : "<exe>", GetLastError()));
					return TRUE;
				}

				module_info.handle = reinterpret_cast<DWORD>(module);
				module_info.size = info.SizeOfImage;
				module_info.name = name;
				return FALSE;
			}

			Sleep(100);
			timeout += 100;

			if (timeout >= 30000)
			{
				common::log("Main", "Failed to find module: "s + (name ? name : "<exe>"));
				return TRUE;
			}
		}

		return FALSE;
	}

	DWORD WINAPI find_game_window_by_class([[maybe_unused]] LPVOID lpParam)
	{
		std::uint32_t T = 0;

		char exe_path[MAX_PATH]; GetModuleFileNameA(nullptr, exe_path, MAX_PATH);

		common::log("Main", "Waiting for window with classname containing 'Valve001'...", common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		{
			while (!glob::main_window)
			{
				EnumWindows(enum_windows_proc, static_cast<LPARAM>(GetCurrentProcessId()));
				if (!glob::main_window) {
					Sleep(1u); T += 1u;
				}

				if (T >= 30000)
				{
					Beep(300, 100); Sleep(100); Beep(200, 100);
					common::log("Main", "Could not find 'Valve001' Window. Not loading RTX Compatibility Mod.", common::LOG_TYPE::LOG_TYPE_ERROR, true);
					return TRUE;
				}
			}
		}

		if (!common::flags::has_flag("nobeep")) {
			Beep(523, 100);
		}

		get_module_handle_and_size(game::shaderapidx9_module, "shaderapidx9.dll", T);
		get_module_handle_and_size(game::studiorender_module, "studiorender.dll", T);
		// get_module_handle_and_size(game::materialsystem_module, "materialsystem.dll", T);
		get_module_handle_and_size(game::engine_module, "engine.dll", T);
		get_module_handle_and_size(game::client_module, "client.dll", T);
		get_module_handle_and_size(game::server_module, "server.dll", T);
		get_module_handle_and_size(game::vstdlib_module, "vstdlib.dll", T);

		// wait a little ..
		Sleep(1u);

		p2::main();
		return 0;
	}
}

namespace tier0
{
	void(__cdecl* OriginalMsg)(const char*, ...) = nullptr;
	void Msg_hk(const char* fmt, ...)
	{
		char buffer[4096];

		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		common::log("Game:Msg >", buffer, common::LOG_TYPE::LOG_TYPE_DEFAULT, true, false, true);

		globals::detoured_msg_fn_origin = true;
		OriginalMsg("%s", buffer);
		globals::detoured_msg_fn_origin = false;
	}

	void(__cdecl* OriginalWarning)(const char*, ...) = nullptr;
	void Warning_hk(const char* fmt, ...)
	{
		char buffer[4096];

		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		common::log("Game:Warning >", buffer, common::LOG_TYPE::LOG_TYPE_WARN, true, false, true);

		globals::detoured_warning_fn_origin = true;
		OriginalWarning("%s", buffer);
		globals::detoured_warning_fn_origin = false;
	}

#ifdef DEBUG
	void(__cdecl* OriginalOM_TimestampedLog)(const char*, ...) = nullptr;
	void COM_TimestampedLog_hk(const char* fmt, ...)
	{
		char buffer[4096];

		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		// Ensure the string ends with '\n'
		size_t len = strlen(buffer);
		if (len == 0 || buffer[len - 1] != '\n')
		{
			if (len < sizeof(buffer) - 1)
			{
				buffer[len] = '\n';
				buffer[len + 1] = '\0';
			}
		}

		common::log("Game:Log >", buffer, common::LOG_TYPE::LOG_TYPE_DEFAULT, true, false, true);

		globals::detoured_com_timestamped_log_origin = true;
		OriginalOM_TimestampedLog("%s", buffer);
		globals::detoured_com_timestamped_log_origin = false;
	}
#endif
}

BOOL APIENTRY DllMain(HMODULE hmodule, const DWORD ul_reason_for_call, LPVOID)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) 
	{
		common::console();
		globals::setup_dll_module(hmodule);
		globals::setup_exe_module();
		globals::setup_homepath();
		common::crash_handler::install();

		common::set_console_color_blue(true);
		common::console_out << "\n  Launching Portal 2 RTX Remix Compatiblity Mod Version [" << COMP_MOD_VERSION_MAJOR << "." << COMP_MOD_VERSION_MINOR << "." << COMP_MOD_VERSION_PATCH << "]";

		if constexpr (COMP_MOD_PRE_RELEASE_NUM != 0) {
			common::console_out << "   - Pre-Release " << std::to_string(COMP_MOD_PRE_RELEASE_NUM) << "\n";
		} else {
			common::console_out << "\n";
		}

		common::console_out << "  > Compiled On : " + std::string(__DATE__) + " " + std::string(__TIME__) + "\n";
		common::console_out << "  > https://github.com/xoxor4d/p2-rtx\n\n";
		common::set_console_color_default();

		if (const auto MH_INIT_STATUS = MH_Initialize(); MH_INIT_STATUS != MH_STATUS::MH_OK)
		{
			log("Main", std::format("MinHook failed to initialize with code: {:d}", static_cast<int>(MH_INIT_STATUS)), common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return TRUE;
		}

		// hook OutputDebugString
		game::SetupDebugOutputHook();

		MH_CreateHookApi(L"tier0.dll", "Warning", tier0::Warning_hk, (LPVOID*)&tier0::OriginalWarning);
		MH_CreateHookApi(L"tier0.dll", "Msg", tier0::Msg_hk, (LPVOID*)&tier0::OriginalMsg);

#ifdef DEBUG // Not something the general user needs to see but very good for debugging
		MH_CreateHookApi(L"tier0.dll", "COM_TimestampedLog", tier0::COM_TimestampedLog_hk, (LPVOID*)&tier0::OriginalOM_TimestampedLog);
#endif
		MH_EnableHook(MH_ALL_HOOKS);

		// init early modules here <>

		if (const auto t = CreateThread(nullptr, 0, p2::find_game_window_by_class, nullptr, 0, nullptr); t) {
			CloseHandle(t);
		}
	}

	return TRUE;
}
