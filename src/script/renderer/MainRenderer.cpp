#include "anki/script/Common.h"
#include "anki/renderer/MainRenderer.h"
#include "anki/renderer/Dbg.h"
#include "anki/renderer/Deformer.h"

ANKI_WRAP(MainRenderer)
{
	class_<MainRenderer, bases<Renderer>, noncopyable>("MainRenderer",
		no_init)
		.def("getDbg", (Dbg& (MainRenderer::*)())(
			&MainRenderer::getDbg),
			return_value_policy<reference_existing_object>())

		.def("getDbgTime", &MainRenderer::getDbgTime)
	;
}