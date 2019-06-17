#include <iostream>

#include "Pluto/Libraries/GLFW/GLFW.hxx"
#include "SpaceMouse.hxx"

// YUCK! 
#ifdef WIN32

#include <windows.h>
// #include "stdafx.h"
#include <tchar.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <crtdbg.h>

#include "spwmacro.h"
#include "si.h"
#include "siapp.h"
#include "spwerror.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

#endif

/* We need GLFW to recieve events via HWND. */

namespace Libraries
{

#ifdef _WIN32
SiHdl devHdl = nullptr;      /* Handle to 3D Mouse Device */
SiSpwEvent Event = {};    /* SpaceWare Event */ 
#endif

SpaceMouse *SpaceMouse::Get()
{
	static SpaceMouse instance;
	if (!instance.is_initialized())
	{
		#ifdef WIN32
		// Initialize here... 
		if (SiInitialize() == SPW_DLL_LOAD_ERROR)  {
			std::cout<<"SpaceMouse error: DLL Load Error."<<std::endl;
			SiTerminate();
			return &instance;
		}

		instance.initialized = true;
		#endif		
	}
	return &instance;
}

SpaceMouse::SpaceMouse() {}

SpaceMouse::~SpaceMouse()
{
#ifdef _WIN32
	// Shutdown here...
	if (initialized) SiTerminate();
#endif
}

bool SpaceMouse::connect_to_window(std::string window)
{
	#ifdef WIN32
	auto glfw = GLFW::Get();
	if (!glfw->is_initialized()) return false;

	auto ptr = glfw->get_ptr(window);
	if (ptr == nullptr) return false;

	auto hWnd = glfwGetWin32Window(ptr);

	SiOpenDataEx oData;                    /* OS Independent data to open ball  */ 
	SiOpenWinInitEx (&oData, hWnd);    /* init Win. platform specific data  */

	// Tell the driver we want to receive V3DCMDs instead of V3DKeys
	SiOpenWinAddHintBoolEnum(&oData, SI_HINT_USESV3DCMDS, SPW_TRUE);

	// Tell the driver we need a min driver version of 17.5.5.  
	// This could be used to tell the driver that it needs to be upgraded before it can run this application correctly.
	SiOpenWinAddHintStringEnum(&oData, SI_HINT_DRIVERVERSION, L"17.5.5");

	/* open data, which will check for device type and return the device handle
	to be used by this function */
	if ((devHdl = SiOpenEx(L"3DxTest", SI_ANY_DEVICE, SI_NO_MASK, SI_EVENT, &oData)) == NULL)
	{
		// std::cout<<"SpaceMouse error, cannot retrieve device handle."<<std::endl;
		return false;
	}
	else
	{
		SiDeviceName devName;
		SiGetDeviceName(devHdl, &devName);
		// std::cout<<"Device found: name " << devName.name <<std::endl;
		
		
		return true;
	}

	#endif
	return true;
}

glm::vec3 SpaceMouse::get_translation()
{
	return translation;
}

glm::vec3 SpaceMouse::get_rotation()
{
	return rotation;
}

void SpaceMouse::SbMotionEvent()
{
#ifdef _WIN32
	translation.x = (float) Event.u.spwData.mData[SI_TX] / 1000.0f;
	translation.y = (float) Event.u.spwData.mData[SI_TY] / 1000.0f;
	translation.z = (float) Event.u.spwData.mData[SI_TZ] / 1000.0f;
	rotation.x = (float) Event.u.spwData.mData[SI_RX] / 1000.0f;
	rotation.y = (float) Event.u.spwData.mData[SI_RY] / 1000.0f;
	rotation.z = (float) Event.u.spwData.mData[SI_RZ] / 1000.0f;
#endif
}

void SpaceMouse::SbZeroEvent()
{

}

void SpaceMouse::HandleDeviceChangeEvent()
{

}

void SpaceMouse::HandleV3DCMDEvent()
{

}

void SpaceMouse::HandleAppEvent() 
{

}

bool SpaceMouse::poll_event()
{
	translation = glm::vec3(0.0);
	rotation = glm::vec3(0.0);
#ifdef WIN32
	if (devHdl == nullptr) return false;

	SiGetEventData EData;    /* SpaceWare Event Data */

	MSG            msg;      /* incoming message to be evaluated */
	BOOL           handled;  /* is message handled yet */ 

	handled = SPW_FALSE;     /* init handled */

	BOOL res;
	// UINT_PTR timerId = SetTimer(NULL, NULL, 0, NULL);
	
	// res = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);

	/* start message loop */ 
	res = GetMessage( &msg, NULL, 0, 0 );

	// KillTimer(NULL, timerId);

	if (!res) {
		return false;
	}

	/* init Window platform specific data for a call to SiGetEvent */
	SiGetEventWinInit(&EData, msg.message, msg.wParam, msg.lParam);

	/* check whether msg was a 3D mouse event and process it */
	if (SiGetEvent (devHdl, SI_AVERAGE_EVENTS, &EData, &Event) == SI_IS_EVENT)
	{
		if (Event.type == SI_MOTION_EVENT)
		{
			SbMotionEvent();        /* process 3D mouse motion event */     
		}
		else if (Event.type == SI_ZERO_EVENT)
		{
			SbZeroEvent();                /* process 3D mouse zero event */     
		}
		else if (Event.type == SI_DEVICE_CHANGE_EVENT)
		{
			HandleDeviceChangeEvent(); /* process 3D mouse device change event */
		}
		else if (Event.type == SI_CMD_EVENT)
		{
			HandleV3DCMDEvent(); /* V3DCMD_* events */
		}
		else if (Event.type == SI_APP_EVENT)
		{
			HandleAppEvent(); /* AppCommand* events */
		}

		handled = SPW_TRUE;              /* 3D mouse event handled */ 
	}

	/* not a 3D mouse event, let windows handle it */
	if (handled == SPW_FALSE)
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	// return( (int) msg.wParam );
	
	return true;
#endif
	return false;
}


}
