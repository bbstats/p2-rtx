#pragma once

namespace common::crash_handler
{
	// Installs a vectored exception handler that writes a crash report and a minidump
	// to 'portal2-rtx/logs/' when a fatal exception occurs. Disable with '-no_crash_handler'.
	void install();
}
