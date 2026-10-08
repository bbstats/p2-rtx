#include "std_include.hpp"
#include "crash_handler.hpp"
#include "components/modules/map_settings.hpp"

#include <DbgHelp.h>
#include <Psapi.h>
#pragma comment(lib, "dbghelp.lib")

// The engine wraps its main loop in a __try/__except that writes a steam minidump and silently exits,
// so an unhandled exception filter never sees most crashes. A vectored handler sees the exception first.
// Everything in the reporting path avoids the CRT heap / std::string because the heap might be what is broken.

namespace common::crash_handler
{
	namespace
	{
		volatile LONG g_reported = 0;

		struct crash_job_s
		{
			EXCEPTION_POINTERS* exception = nullptr;
			DWORD thread_id = 0u;
		};

		crash_job_s g_job = {};

		bool is_fatal(const DWORD code)
		{
			switch (code)
			{
			case EXCEPTION_ACCESS_VIOLATION:
			case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			case EXCEPTION_DATATYPE_MISALIGNMENT:
			case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			case EXCEPTION_ILLEGAL_INSTRUCTION:
			case EXCEPTION_IN_PAGE_ERROR:
			case EXCEPTION_INT_DIVIDE_BY_ZERO:
			case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			case EXCEPTION_PRIV_INSTRUCTION:
			case EXCEPTION_STACK_OVERFLOW:
				return true;

			default:
				return false;
			}
		}

		const char* get_exception_name(const DWORD code)
		{
			switch (code)
			{
			case EXCEPTION_ACCESS_VIOLATION:		 return "ACCESS_VIOLATION";
			case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:	 return "ARRAY_BOUNDS_EXCEEDED";
			case EXCEPTION_DATATYPE_MISALIGNMENT:	 return "DATATYPE_MISALIGNMENT";
			case EXCEPTION_FLT_DIVIDE_BY_ZERO:		 return "FLT_DIVIDE_BY_ZERO";
			case EXCEPTION_ILLEGAL_INSTRUCTION:		 return "ILLEGAL_INSTRUCTION";
			case EXCEPTION_IN_PAGE_ERROR:			 return "IN_PAGE_ERROR";
			case EXCEPTION_INT_DIVIDE_BY_ZERO:		 return "INT_DIVIDE_BY_ZERO";
			case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
			case EXCEPTION_PRIV_INSTRUCTION:		 return "PRIV_INSTRUCTION";
			case EXCEPTION_STACK_OVERFLOW:			 return "STACK_OVERFLOW";
			default:								 return "UNKNOWN";
			}
		}

		// writes to the crash report file and mirrors the text to the external console
		class report_writer
		{
		public:
			explicit report_writer(const char* path) {
				m_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			}

			~report_writer()
			{
				if (m_file != INVALID_HANDLE_VALUE) {
					CloseHandle(m_file);
				}
			}

			void print(const char* fmt, ...)
			{
				char buffer[1024];

				va_list args;
				va_start(args, fmt);
				const int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
				va_end(args);

				if (len <= 0) {
					return;
				}

				const auto size = static_cast<DWORD>(std::min(static_cast<size_t>(len), sizeof(buffer) - 1u));
				if (m_file != INVALID_HANDLE_VALUE)
				{
					DWORD written = 0u;
					WriteFile(m_file, buffer, size, &written, nullptr);
				}

				console_write(std::string_view(buffer, size));
			}

		private:
			HANDLE m_file = INVALID_HANDLE_VALUE;
		};

		// module+offset of an address, e.g. 'client.dll+0x1A2B3C'
		void describe_address(const DWORD address, char* out, const size_t out_size)
		{
			HMODULE mod = nullptr;
			if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &mod) && mod)
			{
				char path[MAX_PATH] = {};
				GetModuleFileNameA(mod, path, MAX_PATH);

				const char* name = strrchr(path, '\\');
				name = name ? name + 1 : path;

				snprintf(out, out_size, "%s+0x%X", name, static_cast<unsigned>(address - reinterpret_cast<DWORD>(mod)));
				return;
			}

			snprintf(out, out_size, "0x%08X (unknown module)", static_cast<unsigned>(address));
		}

		void get_current_map_name(char* out, const size_t out_size)
		{
			out[0] = '\0';
			__try
			{
				strncpy_s(out, out_size, components::map_settings::get_map_name().c_str(), _TRUNCATE);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {
				out[0] = '\0';
			}
		}

		void write_stack(report_writer& r, const CONTEXT* exception_context, const DWORD thread_id)
		{
			const auto process = GetCurrentProcess();
			const auto thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, thread_id);

			SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_UNDNAME);
			const bool has_symbols = SymInitialize(process, nullptr, TRUE);

			CONTEXT context = *exception_context;
			STACKFRAME64 frame = {};
			frame.AddrPC.Offset = context.Eip;		frame.AddrPC.Mode = AddrModeFlat;
			frame.AddrFrame.Offset = context.Ebp;	frame.AddrFrame.Mode = AddrModeFlat;
			frame.AddrStack.Offset = context.Esp;	frame.AddrStack.Mode = AddrModeFlat;

			alignas(SYMBOL_INFO) char symbol_buffer[sizeof(SYMBOL_INFO) + 256] = {};
			const auto symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);

			for (auto i = 0; i < 48; i++)
			{
				if (!StackWalk64(IMAGE_FILE_MACHINE_I386, process, thread ? thread : GetCurrentThread(), &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
					break;
				}

				if (!frame.AddrPC.Offset) {
					break;
				}

				char location[MAX_PATH + 32];
				describe_address(static_cast<DWORD>(frame.AddrPC.Offset), location, sizeof(location));

				symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
				symbol->MaxNameLen = 255;

				DWORD64 displacement = 0u;
				if (has_symbols && SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
					r.print("  #%02d  %s  (%s+0x%llX)\n", i, location, symbol->Name, displacement);
				} else {
					r.print("  #%02d  %s\n", i, location);
				}
			}

			if (has_symbols) {
				SymCleanup(process);
			}

			if (thread) {
				CloseHandle(thread);
			}
		}

		void write_modules(report_writer& r)
		{
			HMODULE modules[512];
			DWORD needed = 0u;

			if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
				return;
			}

			const auto count = std::min(static_cast<size_t>(needed / sizeof(HMODULE)), std::size(modules));
			for (auto i = 0u; i < count; i++)
			{
				char path[MAX_PATH] = {};
				GetModuleFileNameA(modules[i], path, MAX_PATH);

				MODULEINFO info = {};
				GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof(info));

				r.print("  0x%08X - 0x%08X  %s\n", reinterpret_cast<DWORD>(info.lpBaseOfDll), reinterpret_cast<DWORD>(info.lpBaseOfDll) + info.SizeOfImage, path);
			}
		}

		DWORD WINAPI write_crash_report(LPVOID)
		{
			const auto ex = g_job.exception;
			const auto record = ex->ExceptionRecord;

			char logs_dir[MAX_PATH];
			snprintf(logs_dir, sizeof(logs_dir), "%s" COMPMOD_ASSET_DIR "logs", globals::root_path.c_str());
			CreateDirectoryA(logs_dir, nullptr);

			SYSTEMTIME t;
			GetLocalTime(&t);

			char base_path[MAX_PATH];
			snprintf(base_path, sizeof(base_path), "%s\\crash_%04d-%02d-%02d_%02d-%02d-%02d", logs_dir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

			char report_path[MAX_PATH], dump_path[MAX_PATH];
			snprintf(report_path, sizeof(report_path), "%s.txt", base_path);
			snprintf(dump_path, sizeof(dump_path), "%s.dmp", base_path);

			// minidump first - most valuable if anything below fails
			bool dump_written = false;
			if (const auto file = CreateFileA(dump_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				file != INVALID_HANDLE_VALUE)
			{
				MINIDUMP_EXCEPTION_INFORMATION info = { g_job.thread_id, ex, FALSE };
				const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);

				dump_written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, &info, nullptr, nullptr);
				CloseHandle(file);

				if (!dump_written) {
					DeleteFileA(dump_path);
				}
			}

			report_writer r(report_path);

			r.print("\n========================================================================\n");
			r.print(" Portal 2 RTX Compatibility Mod [%d.%d.%d] - FATAL EXCEPTION\n", COMP_MOD_VERSION_MAJOR, COMP_MOD_VERSION_MINOR, COMP_MOD_VERSION_PATCH);
			r.print(" Compiled On: %s %s\n", __DATE__, __TIME__);
			r.print("========================================================================\n");
			r.print(" Please attach this file, '%s.dmp' and 'logfile.txt' (same folder) when reporting a crash:\n", base_path);
			r.print(" > https://github.com/xoxor4d/p2-rtx/issues\n");
			r.print(" If the game kept running, the game handled this exception and you can ignore this report.\n\n");

			char location[MAX_PATH + 32];
			describe_address(reinterpret_cast<DWORD>(record->ExceptionAddress), location, sizeof(location));

			r.print("Exception:  0x%08X (%s)\n", static_cast<unsigned>(record->ExceptionCode), get_exception_name(record->ExceptionCode));
			r.print("Location:   %s\n", location);

			if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
			{
				const auto op = record->ExceptionInformation[0];
				r.print("Access:     %s address 0x%08X\n", op == 0 ? "read from" : op == 1 ? "write to" : "execute at", static_cast<DWORD>(record->ExceptionInformation[1]));
			}

			char map_name[128];
			get_current_map_name(map_name, sizeof(map_name));
			r.print("Map:        %s\n", map_name[0] ? map_name : "<none>");
			r.print("Thread:     %u%s\n", static_cast<unsigned>(g_job.thread_id), g_job.thread_id == GetWindowThreadProcessId(glob::main_window, nullptr) ? " (main)" : "");
			r.print("Minidump:   %s\n\n", dump_written ? dump_path : "<failed to write>");

			if (const auto c = ex->ContextRecord; c)
			{
				r.print("Registers:\n");
				r.print("  EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", c->Eax, c->Ebx, c->Ecx, c->Edx);
				r.print("  ESI=%08X EDI=%08X EBP=%08X ESP=%08X\n", c->Esi, c->Edi, c->Ebp, c->Esp);
				r.print("  EIP=%08X EFLAGS=%08X\n\n", c->Eip, c->EFlags);

				r.print("Call stack:\n");
				write_stack(r, c, g_job.thread_id);
				r.print("\n");
			}

			r.print("Loaded modules:\n");
			write_modules(r);
			r.print("========================================================================\n\n");

			return 0;
		}

		LONG CALLBACK vectored_exception_handler(EXCEPTION_POINTERS* ex)
		{
			if (!ex || !ex->ExceptionRecord || !is_fatal(ex->ExceptionRecord->ExceptionCode) || IsDebuggerPresent()) {
				return EXCEPTION_CONTINUE_SEARCH;
			}

			// only ever report the first fatal exception
			if (InterlockedExchange(&g_reported, 1) != 0) {
				return EXCEPTION_CONTINUE_SEARCH;
			}

			g_job.exception = ex;
			g_job.thread_id = GetCurrentThreadId();

			// write the report from a fresh thread: the crashing thread might be out of stack (stack overflow)
			// and dbghelp recommends not dumping from the faulting thread
			if (const auto thread = CreateThread(nullptr, 0, write_crash_report, nullptr, 0, nullptr); thread)
			{
				WaitForSingleObject(thread, 30000);
				CloseHandle(thread);
			}

			return EXCEPTION_CONTINUE_SEARCH;
		}

		bool has_opt_out_flag()
		{
			char cmdline[2048] = {};
			strncpy_s(cmdline, GetCommandLineA(), _TRUNCATE);
			_strlwr_s(cmdline);
			return strstr(cmdline, "-no_crash_handler") != nullptr;
		}
	}

	void install()
	{
		if (has_opt_out_flag()) {
			return;
		}

		AddVectoredExceptionHandler(1, vectored_exception_handler);
	}
}
