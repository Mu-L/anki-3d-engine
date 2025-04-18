// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Util/Assert.h>
#include <AnKi/Util/System.h>
#include <AnKi/Util/Functions.h>
#include <cstdlib>
#include <cstdio>
#if ANKI_OS_ANDROID
#	include <android/log.h>
#endif

namespace anki {

#if ANKI_EXTRA_CHECKS

void akassert(const char* exprTxt, const char* file, int line, const char* func)
{
#	if ANKI_OS_ANDROID
	__android_log_print(ANDROID_LOG_ERROR, "AnKi", "Assertion failed: %s (%s:%d %s)", exprTxt, file, line, func);
#	else
#		if ANKI_OS_LINUX
	if(runningFromATerminal())
	{
		fprintf(stderr, "\033[1;31mAssertion failed: %s (%s:%d %s)\033[0m\n", exprTxt, file, line, func);
	}
	else
#		endif
	{
		fprintf(stderr, "Assertion failed: %s (%s:%d %s)\n", exprTxt, file, line, func);
	}
#	endif

	printf("Backtrace:\n");
	U32 count = 0;
	backtrace([&count](CString symbol) {
		printf("%.2u: %s\n", count++, symbol.cstr());
	});

#	if ANKI_OS_WINDOWS
	Array<Char, 512> msg;
	snprintf(msg.getBegin(), msg.getSize(), "%s\n\n%s:%d %s", exprTxt, file, line, func);
	if(!IsDebuggerPresent())
	{
		MessageBoxA(nullptr, msg.getBegin(), "Assert failed", MB_OK | MB_ICONERROR);
	}
#	endif
	ANKI_DEBUG_BREAK();
}

#endif

} // end namespace anki
