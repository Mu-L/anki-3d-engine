// Copyright (C) 2009-present, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <AnKi/Core/CVarSet.h>
#include <AnKi/Util/Xml.h>
#include <AnKi/Util/Logger.h>
#include <AnKi/Util/File.h>

// Because some cvars set their default values
#include <AnKi/Util/System.h>
#include <AnKi/Shaders/Include/ClusteredShadingTypes.h>

namespace anki {

static constexpr Array<CString, U32(CVarSubsystem::kCount)> g_cvarSubsystemNames = {"Core", "R", "Gr", "Rsrc", "Scene"};

void CVar::getFullNameInternal(Array<Char, 256>& arr) const
{
	snprintf(arr.getBegin(), arr.getSize(), "%s.%s", g_cvarSubsystemNames[m_subsystem].cstr(), m_name.cstr());
}

CoreString CVar::getFullName() const
{
	CoreString out;
	out.sprintf("%s.%s", g_cvarSubsystemNames[m_subsystem].cstr(), m_name.cstr());
	return out;
}

void CVarSet::registerCVar(CVar* cvar)
{
	for([[maybe_unused]] CVar& it : m_cvars)
	{
		ANKI_ASSERT(it.m_name != cvar->m_name || it.m_subsystem != cvar->m_subsystem);
	}

	m_cvars.pushBack(cvar);
}

Error CVarSet::setMultiple(ConstWeakArray<const Char*> arr)
{
	for(U32 i = 0; i < arr.getSize(); ++i)
	{
		ANKI_ASSERT(arr[i]);
		const CString varName = arr[i];

		// Get the value string
		++i;
		if(i >= arr.getSize())
		{
			ANKI_CORE_LOGE("Expecting a command line argument after %s", varName.cstr());
			return Error::kUserData;
		}
		ANKI_ASSERT(arr[i]);
		const CString value = arr[i];

		// Find the CVar
		CVar* foundCVar = nullptr;
		Array<Char, 256> fullnameArr;
		for(CVar& it : m_cvars)
		{
			it.getFullNameInternal(fullnameArr);
			CString fullname = &fullnameArr[0];

			if(fullname == varName || it.m_name == varName)
			{
				if(foundCVar)
				{
					ANKI_CORE_LOGE("Command line arg %s has ambiguous name. Skipping", varName.cstr());
				}
				else
				{
					foundCVar = &it;
				}
			}
		}

		if(foundCVar)
		{
#define ANKI_CVAR_NUMERIC_SET(type) \
	case CVar::Type::kNumeric##type: \
	{ \
		type v; \
		err = value.toNumber(v); \
		if(!err) \
		{ \
			static_cast<NumericCVar<type>&>(*foundCVar).set(v); \
		} \
		break; \
	}

			Error err = Error::kNone;
			switch(foundCVar->m_type)
			{
			case CVar::Type::kString:
				static_cast<StringCVar&>(*foundCVar).set(value);
				break;
			case CVar::Type::kBool:
			{
				U32 v;
				err = value.toNumber(v);
				if(!err)
				{
					static_cast<BoolCVar&>(*foundCVar).set(v != 0);
				}
				break;
			}
				ANKI_CVAR_NUMERIC_SET(U8)
				ANKI_CVAR_NUMERIC_SET(U16)
				ANKI_CVAR_NUMERIC_SET(U32)
				ANKI_CVAR_NUMERIC_SET(PtrSize)
				ANKI_CVAR_NUMERIC_SET(F32)
			default:
				ANKI_ASSERT(0);
			}

			if(err)
			{
				foundCVar->getFullNameInternal(fullnameArr);
				ANKI_CORE_LOGE("Wrong value for %s. Value will not be set", &fullnameArr[0]);
			}
		}
		else
		{
			ANKI_CORE_LOGE("Can't recognize command line argument: %s. Skipping", varName.cstr());
		}
	}

	return Error::kNone;
}

} // end namespace anki
