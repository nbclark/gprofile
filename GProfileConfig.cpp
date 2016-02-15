#include <aygshell.h>
#include "GProfileConfig.h"
#include "..\LightProfileMonitor\LightProfileMonitor.h"
#include "Sensor.h"

#define NUM_APPLETS		1
#define	ID_POWEROFFMODE	0
#define	ID_POWERONMODE	1

typedef struct tagApplets
{
    int icon;           // icon resource identifier
    int namestring;     // name-string resource identifier
    int descstring;     // description-string resource identifier
} APPLETS;

const APPLETS SystemApplets[] =
{
    {APPLET_ICON, APPLET_NAME, APPLET_DESC}
	// add more struct members here if supporting more than on applet
};

HINSTANCE	g_hInstance	= NULL;
BOOL CreatePropertySheet(HWND hWnd, int iApplet);
SHORT g_shOrientationMin = 5000, g_shOrientationMax = -5000;
static HWND g_hPowerOff, g_hPowerOn;
static DWORD g_dwDefaultProfile[2], g_dwMinRing[2], g_dwMaxRing[2], g_dwMinVib[2], g_dwMaxVib[2], g_dwPollingInterval[2], g_dwNotify[2], g_dwDisable[2];

////////////////////////////////////////////////////////
//	DllMain
//
////////////////////////////////////////////////////////
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	if (DLL_PROCESS_ATTACH == ul_reason_for_call)
		g_hInstance = (HINSTANCE)hModule;

    return TRUE;
}


////////////////////////////////////////////////////////
//	Do initialization, eg memory allocations, etc.
//  return TRUE if all OK and FALSE if applet shouldn't
//  be loaded
//
////////////////////////////////////////////////////////
BOOL InitApplet(HWND hwndParent)
{
    return TRUE;
}


////////////////////////////////////////////////////////
//	Do cleanup here
//  
////////////////////////////////////////////////////////
void TermApplet()
{
    return;
}

DWORD WINAPI PollSensor( LPVOID lpParam )
{
	SENSORDATATILT sensorData;

	g_shOrientationMin = 5000;
	g_shOrientationMax = -5000;

	while (TRUE)
	{
		if (g_hSensor)
		{
			SensorGetDataTilt(g_hSensor, &sensorData);
			
			if (sensorData.Orientation < g_shOrientationMin)
			{
				g_shOrientationMin = sensorData.Orientation;
			}
			if (sensorData.Orientation > g_shOrientationMax)
			{
				g_shOrientationMax = sensorData.Orientation;
			}
		}
		Sleep(250);
	}
}

////////////////////////////////////////////////////////
//	This is the entry point called by ctlpnl.exe
//  
////////////////////////////////////////////////////////
extern "C"
__declspec(dllexport)
LONG WINAPI CPlApplet(HWND hwndCPL, UINT uMsg, LONG lParam1, LONG lParam2)
{
	static int		iInitCount = 0;
	int				iApplet;
	static DWORD	dwThread = 0;

    switch (uMsg)
	{
		// First message sent. It is sent only once to
		// allow the dll to initialize it's applet(s)
		case CPL_INIT:
			if (!iInitCount)
			{
				if (!InitApplet(hwndCPL))
					return FALSE;
			}
			iInitCount++;
			return TRUE;
			
		// Second message sent. Return the count of applets supported
		// by this dll
		case CPL_GETCOUNT:
			// Return the number of applets we support
			return (LONG)((sizeof(SystemApplets))/(sizeof(SystemApplets[0])));

		// Third message sent. Sent once for each applet supported by this dll.
		// The lParam1 contains the number that indicates which applet this is
		// for, from 0 to 1 less than the count of applets.
		// lParam2 is a NEWCPLINFO that should be filled with information about
		// this applet before returning
		case CPL_NEWINQUIRE:
			{
				LPNEWCPLINFO	lpNewCPlInfo;

				lpNewCPlInfo				= (LPNEWCPLINFO)lParam2;
				iApplet						= (int)lParam1;
				lpNewCPlInfo->dwSize		= (DWORD)sizeof(NEWCPLINFO);
				lpNewCPlInfo->dwFlags		= 0;
				lpNewCPlInfo->dwHelpContext	= 0;
				lpNewCPlInfo->lData			= SystemApplets[iApplet].icon;
				lpNewCPlInfo->hIcon			= LoadIcon(g_hInstance,(LPCTSTR)MAKEINTRESOURCE(SystemApplets[iApplet].icon));
				lpNewCPlInfo->szHelpFile[0]	= '\0';

				LoadString(g_hInstance,SystemApplets[iApplet].namestring,lpNewCPlInfo->szName,32);
				LoadString(g_hInstance,SystemApplets[iApplet].descstring,lpNewCPlInfo->szInfo,64);
			}

			break;

		// This is sent whenever the user clicks an icon in Settings for one of
		// the applets supported by this dll. lParam1 contains the number indicating
		// which applet. Return 0 if applet successfully launched, non-zero otherwise
		case CPL_DBLCLK:
			iApplet = (UINT)lParam1;
			if (!CreatePropertySheet(hwndCPL,iApplet))
				return 1;
			break;
			
		// Sent once per applet, before CPL_EXIT
		case CPL_STOP:
			break;
			
		// Sent once before the dll is unloaded
		case CPL_EXIT:
			iInitCount--;
			if (!iInitCount)
				TermApplet();
			break;
			
		default:
			break;
    }

    return 0;
}

void SetMinMaxLabels(HWND hwndDlg, DWORD shMin, DWORD shMax)
{
	WCHAR wzStatic[200];
	
	_itow(shMin, wzStatic, 10);
	SetDlgItemText(hwndDlg, IDC_STATIC_SET_MIN, wzStatic);
	_itow(shMax, wzStatic, 10);
	SetDlgItemText(hwndDlg, IDC_STATIC_SET_MAX, wzStatic);
}

BOOL CALLBACK ConfigureProc(HWND hwndDlg, 
                             UINT message, 
                             WPARAM wParam, 
                             LPARAM lParam) 
{
	static HWND hWndMenu;
	static HANDLE hThread;
	static int iTimerTicks = 0;

    switch (message) 
    { 
		case WM_INITDIALOG:
		{
			SHMENUBARINFO mbi;
			memset(&mbi, 0, sizeof(SHMENUBARINFO));
			mbi.cbSize = sizeof(SHMENUBARINFO);
			mbi.hwndParent = hwndDlg;
			mbi.nToolBarId = IDR_CARDVIEW_MENUBAR;
			mbi.hInstRes = g_hInstance;
			mbi.nBmpId = 0;
			mbi.cBmpImages = 0;	
		    
			if (FALSE == SHCreateMenuBar(&mbi))
			{
				MessageBox(hwndDlg, L"SHCreateMenuBar Failed", L"Error", MB_OK);
			}
			hWndMenu = mbi.hwndMB;
			
			SHINITDLGINFO shidi;
			// Create a Done button and size it.
			shidi.dwMask = SHIDIM_FLAGS;
			shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN;
			shidi.hDlg = hwndDlg;
			SHInitDialog(&shidi);
			
			SetMinMaxLabels(hwndDlg, g_shOrientationMin, g_shOrientationMax);
		}
		break;
		case WM_DESTROY:
			{
				if (g_hSensor)
				{
					SensorUninit(g_hSensor);
					g_hSensor = NULL;
				}
				if (hThread)
				{
					TerminateThread(hThread, 0);
					hThread = NULL;
				}
				DestroyWindow(hWndMenu);
			}
			break;
		case WM_TIMER:
			{
				if (iTimerTicks == 1)
				{
					// Notify that we are starting
					LedOn(1);
					Sleep(50);
					LedOff(1);
					Sleep(50);
					
					SensorInit(SENSOR_TILT_PORT, &g_hSensor);
					hThread = CreateThread(NULL, 0, PollSensor, NULL, 0, NULL);
				}
				if (iTimerTicks > 5)
				{
					// Notify that we are stopping
					LedOn(1);
					Sleep(50);
					LedOff(1);
					Sleep(50);

					if (g_hSensor)
					{
						SensorUninit(g_hSensor);
						g_hSensor = NULL;
					}
					if (hThread)
					{
						TerminateThread(hThread, 0);
						hThread = NULL;
					}

					SetMinMaxLabels(hwndDlg, g_shOrientationMin, g_shOrientationMax);

					KillTimer(hwndDlg, 1);
					EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_SET), TRUE);
				}
				iTimerTicks++;
			}
			break;
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            {
				case IDC_BUTTON_SET :
					{
						iTimerTicks = 0;
						EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_SET), FALSE);
						SetTimer(hwndDlg, 1, 1000, NULL);
					}
				break;
				case IDOK:
					{
						EndDialog(hwndDlg, IDOK);
					}
                    break;
				case IDM_CARDVIEW_SK1_ACCEPT:
					{
						EndDialog(hwndDlg, IDOK);
					}
                    break;
				case IDM_CARDVIEW_SK2_CANCEL:
					{
						EndDialog(hwndDlg, IDCANCEL);
					}
                    break;
            }
    } 
    return FALSE; 
}

void SetRadioButtons(HWND hwndDlg, BOOL bVibVal, BOOL bRingVal)
{
	EnableWindow(GetDlgItem(hwndDlg, IDC_STATIC_VIBLBL), bVibVal);
	EnableWindow(GetDlgItem(hwndDlg, IDC_STATIC_VIBVAL), bVibVal);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_VIB), bVibVal);
	
	EnableWindow(GetDlgItem(hwndDlg, IDC_STATIC_RINGLBL), bRingVal);
	EnableWindow(GetDlgItem(hwndDlg, IDC_STATIC_RINGVAL), bRingVal);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RING), bRingVal);
}

int CALLBACK PowerModePageProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	HWND	hwndRadioRing, hwndRadioVibrate, hwndRadioNone, hwndPollInterval;
	DWORD	dwCtrlId;

	LPPROPSHEETPAGE ppsp;
	TCHAR	szStatic[16];
	static HWND hWndMenu;
	REGISTRYOP regOp;
	int iPage;

	BOOL bVibVal = FALSE, bRingVal = FALSE;

	if (hwndDlg == g_hPowerOff)
	{
		regOp = eScreenOff;
		iPage = ID_POWEROFFMODE;
	}
	if (hwndDlg == g_hPowerOn)
	{
		regOp = eScreenOn;
		iPage = ID_POWERONMODE;
	}
	
	switch (uMsg)
	{
		case WM_INITDIALOG:

			// lParam contains the PROPSHEETPAGE struct; store the HWND into the dword at the end of the struct
			ppsp = (LPPROPSHEETPAGE)lParam;
			*((HWND*)(ppsp + 1)) = hwndDlg;

			iPage = (int)ppsp->lParam;
			switch (iPage)
			{
				case ID_POWEROFFMODE:
					g_hPowerOff = hwndDlg;
					regOp = eScreenOff;

					break;
				// Break intentionally left out so code falls through
				case ID_POWERONMODE:
					g_hPowerOn = hwndDlg;
					regOp = eScreenOn;

					break;
			}

			SHMENUBARINFO mbi;
			memset(&mbi, 0, sizeof(SHMENUBARINFO));
			mbi.cbSize = sizeof(SHMENUBARINFO);
			mbi.hwndParent = hwndDlg;
			mbi.nToolBarId = 0;
			mbi.hInstRes = g_hInstance;
			mbi.nBmpId = 0;
			mbi.cBmpImages = 0;	
		    
			if (FALSE == SHCreateMenuBar(&mbi))
			{
				MessageBox(hwndDlg, L"SHCreateMenuBar Failed", L"Error", MB_OK);
			}
			hWndMenu = mbi.hwndMB;

			// get current slection
			GetFromRegistry(regOp, REG_DEFAULTPROFILE, &g_dwDefaultProfile[iPage]);
			GetFromRegistry(regOp, REG_MINORIENTATIONRING, &g_dwMinRing[iPage]);
			GetFromRegistry(regOp, REG_MAXORIENTATIONRING, &g_dwMaxRing[iPage]);
			GetFromRegistry(regOp, REG_MINORIENTATIONVIB, &g_dwMinVib[iPage]);
			GetFromRegistry(regOp, REG_MAXORIENTATIONVIB, &g_dwMaxVib[iPage]);
			GetFromRegistry(regOp, REG_POLLINGINTERVAL, &g_dwPollingInterval[iPage]);
			GetFromRegistry(regOp, REG_NOTIFY, &g_dwNotify[iPage]);
			GetFromRegistry(regOp, REG_DISABLE, &g_dwDisable[iPage]);

			hwndRadioRing = GetDlgItem(hwndDlg,IDC_RADIO_RING);
			hwndRadioVibrate = GetDlgItem(hwndDlg,IDC_RADIO_VIBRATE);
			hwndRadioNone = GetDlgItem(hwndDlg,IDC_RADIO_NONE);

			hwndPollInterval = GetDlgItem(hwndDlg, IDC_COMBO1);

			SendMessage(hwndRadioVibrate,BM_SETCHECK,(WPARAM)(g_dwDefaultProfile[iPage] == 0 ? BST_CHECKED : BST_UNCHECKED),0);
			SendMessage(hwndRadioRing,BM_SETCHECK,(WPARAM)(g_dwDefaultProfile[iPage] == 1 ? BST_CHECKED : BST_UNCHECKED),0);
			SendMessage(hwndRadioNone,BM_SETCHECK,(WPARAM)(g_dwDefaultProfile[iPage] == 2 ? BST_CHECKED : BST_UNCHECKED),0);
			
			SendDlgItemMessage(hwndDlg, IDC_CHECK_NOTIFY, BM_SETCHECK, (WPARAM)(g_dwNotify[iPage] == 0 ? BST_UNCHECKED : BST_CHECKED),0);
			SendDlgItemMessage(hwndDlg, IDC_CHECK_DISABLE, BM_SETCHECK, (WPARAM)(g_dwDisable[iPage] == 0 ? BST_UNCHECKED : BST_CHECKED),0);

			SetRadioButtons(hwndDlg, g_dwDefaultProfile[iPage] != 0, g_dwDefaultProfile[iPage] != 1);

			wsprintf(szStatic, L"%d to %d", g_dwMinRing[iPage], g_dwMaxRing[iPage]);
			SetDlgItemText(hwndDlg, IDC_STATIC_RINGVAL, szStatic);

			wsprintf(szStatic, L"%d to %d", g_dwMinVib[iPage], g_dwMaxVib[iPage]);
			SetDlgItemText(hwndDlg, IDC_STATIC_VIBVAL, szStatic);

			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"5 Seconds"), 5);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"15 Seconds"), 15);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"30 Seconds"), 30);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"60 Seconds"), 60);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"90 Seconds"), 90);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"120 Seconds"), 120);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"180 Seconds"), 180);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"300 Seconds"), 300);
			SendMessage(hwndPollInterval, CB_SETITEMDATA, SendMessage(hwndPollInterval, CB_ADDSTRING, NULL, (LPARAM)L"600 Seconds"), 600);

			wsprintf(szStatic, L"%d Seconds", g_dwPollingInterval[iPage]);

			g_dwPollingInterval[iPage] = SendMessage(hwndPollInterval, CB_FINDSTRINGEXACT, -1, (LPARAM)szStatic); 

			if (CB_ERR == g_dwPollingInterval[iPage])
			{
				g_dwPollingInterval[iPage] = 3;
			}

			SendMessage(hwndPollInterval, CB_SETCURSEL, g_dwPollingInterval[iPage], NULL);

			return TRUE;

		case WM_COMMAND :
			{
				switch (HIWORD(wParam))
				{
					case BN_CLICKED :
						{
							dwCtrlId = LOWORD(wParam);

							if (dwCtrlId == IDC_BUTTON_VIB || dwCtrlId == IDC_BUTTON_RING)
							{
								if (dwCtrlId == IDC_BUTTON_VIB)
								{
									g_shOrientationMin = (SHORT)g_dwMinVib[iPage];
									g_shOrientationMax = (SHORT)g_dwMaxVib[iPage];
								}
								else
								{
									g_shOrientationMin = (SHORT)g_dwMinRing[iPage];
									g_shOrientationMax = (SHORT)g_dwMaxRing[iPage];
								}

								if (IDOK == DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_SET), hwndDlg, (DLGPROC)ConfigureProc))
								{
									wsprintf(szStatic, L"%d to %d", g_shOrientationMin, g_shOrientationMax);

									if (dwCtrlId == IDC_BUTTON_VIB)
									{
										g_dwMinVib[iPage] = g_shOrientationMin;
										g_dwMaxVib[iPage] = g_shOrientationMax;
										SetDlgItemText(hwndDlg, IDC_STATIC_VIBVAL, szStatic);
									}
									else
									{
										g_dwMinRing[iPage] = g_shOrientationMin;
										g_dwMaxRing[iPage] = g_shOrientationMax;
										SetDlgItemText(hwndDlg, IDC_STATIC_RINGVAL, szStatic);
									}
								}

							}
							
							if (dwCtrlId == IDC_RADIO_NONE)
							{
								bVibVal = TRUE;
								bRingVal = TRUE;
							}
							if (dwCtrlId == IDC_RADIO_RING)
							{
								bVibVal = TRUE;
								bRingVal = FALSE;
							}
							if (dwCtrlId == IDC_RADIO_VIBRATE)
							{
								bVibVal = FALSE;
								bRingVal = TRUE;
							}

							if (bVibVal || bRingVal)
							{
								SetRadioButtons(hwndDlg, bVibVal, bRingVal);
							}
						}
						break;
				}
			}
			break;		
		
		case WM_HSCROLL:            // track bar message
			switch LOWORD(wParam) 
			{
				case TB_BOTTOM:
				case TB_THUMBPOSITION:
				case TB_LINEUP:
				case TB_LINEDOWN:
				case TB_PAGEUP:
				case TB_PAGEDOWN:
				case TB_TOP:
					{
					}

					return TRUE;
				default:
					// Default case
					return FALSE;
			}
			break;
	}

	return FALSE;
}


////////////////////////////////////////////////////////
//	The DialogProc for the On AC Power property page
//
////////////////////////////////////////////////////////
int CALLBACK PowerOnModePageProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	LPPROPSHEETPAGE ppsp;

	switch (uMsg)
	{
		case WM_INITDIALOG:        
			// get current slection
			// lParam contains the PROPSHEETPAGE struct; store the HWND into the dword at the end of the struct
			ppsp = (LPPROPSHEETPAGE)lParam;
			*((HWND*)(ppsp + 1)) = hwndDlg;

			return TRUE;

		case WM_HSCROLL:            // track bar message
			switch LOWORD(wParam) 
			{
				// Breaks 
				case TB_BOTTOM:
				case TB_THUMBPOSITION:
				case TB_LINEUP:
				case TB_LINEDOWN:
				case TB_PAGEUP:
				case TB_PAGEDOWN:
				case TB_TOP:
					{
					}
					return TRUE;
				default:
					// Default case
					return FALSE;
			}
			break;
	}

	return FALSE;
}


////////////////////////////////////////////////////////
//	This callback is called when each property page gets
//  created and when it gets released.
////////////////////////////////////////////////////////
UINT CALLBACK PropSheetPageProc(HWND hwnd,UINT uMsg,LPPROPSHEETPAGE ppsp)
{
	HWND	hwndDlg;
	DWORD	dwVibrate;
	DWORD	g_dwPollingInterval;
	REGISTRYOP regOp;

	// Get the HWND of this property page that was tucked at the
	// end of the propsheetpage struct in the WM_INITDIALOG for this page
	hwndDlg = *((HWND*)(ppsp + 1));

	switch(uMsg)
	{
	case PSPCB_CREATE:
		//Return any non zero value to indicate success
		return 1;
		break;

	case PSPCB_RELEASE:
		//Make this a code block so declaration of iPage is local
		{
			int iPage = (int)ppsp->lParam;
			switch (iPage)
			{
				case ID_POWEROFFMODE:
					regOp = eScreenOff;

					break;
				// Break intentionally left out so code falls through
				case ID_POWERONMODE:
					regOp = eScreenOn;

					break;

				default:
					// We should never see this case.  If we do we want to know in debug builds.
					ASSERT(false);
					break;
			}
			
			if (SendDlgItemMessage(hwndDlg, IDC_RADIO_VIBRATE, BM_GETCHECK, 0, 0))
			{
				dwVibrate = 0;
			}
			else if (SendDlgItemMessage(hwndDlg, IDC_RADIO_RING, BM_GETCHECK, 0, 0))
			{
				dwVibrate = 1;
			}
			else if (SendDlgItemMessage(hwndDlg, IDC_RADIO_NONE, BM_GETCHECK, 0, 0))
			{
				dwVibrate = 2;
			}

			SetToRegistry(regOp, REG_DEFAULTPROFILE, dwVibrate);
			
			g_dwPollingInterval = SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_GETITEMDATA, SendDlgItemMessage(hwndDlg, IDC_COMBO1, CB_GETCURSEL, 0, 0), 0);

			SetToRegistry(regOp, REG_MINORIENTATIONRING, g_dwMinRing[iPage]);
			SetToRegistry(regOp, REG_MAXORIENTATIONRING, g_dwMaxRing[iPage]);
			SetToRegistry(regOp, REG_MINORIENTATIONVIB, g_dwMinVib[iPage]);
			SetToRegistry(regOp, REG_MAXORIENTATIONVIB, g_dwMaxVib[iPage]);
			SetToRegistry(regOp, REG_POLLINGINTERVAL, g_dwPollingInterval);
			SetToRegistry(regOp, REG_NOTIFY, SendDlgItemMessage(hwndDlg, IDC_CHECK_NOTIFY, BM_GETCHECK, 0, 0) ? 1 : 0);
			SetToRegistry(regOp, REG_DISABLE, SendDlgItemMessage(hwndDlg, IDC_CHECK_DISABLE, BM_GETCHECK, 0, 0) ? 1 : 0);

			// return value is ignored for this message
			return 0;
		}
		break;
	default:
		// We should never see this case.  If we do we want to know in debug builds.
		ASSERT(false);
		break;
	}
	//return for default case above
	return (UINT)-1;
}


////////////////////////////////////////////////////////
//	The PropSheetProc for the property sheet.
//
////////////////////////////////////////////////////////
int CALLBACK PropSheetProc(HWND hwndDlg,UINT uMsg,LPARAM lParam)
{
	switch (uMsg)
	{
		// Returning COMCTL32_VERSION to this message makes the
		// style of the property sheet to be that of the native
		// Pocket PC ones
		case PSCB_GETVERSION:
			return COMCTL32_VERSION;
		
		// The text copied into the lParam on receipt of this message
		// is displayed as the title of the propertysheet
		case PSCB_GETTITLE:
			wcscpy((TCHAR*)lParam,TEXT("GProfile Settings"));
			break;
		
		// The text copied into the lParam on receipt of this message
		// gets displayed as the link text at the bottom of the
		// propertysheet
		case PSCB_GETLINKTEXT:
			//wcscpy((TCHAR*)lParam,TEXT("Go to <file:ctlpnl.exe cplmain.cpl,2,1{another}> applet."));
			break;
			
		default:
			break;
	}
	
	return 0;
}


////////////////////////////////////////////////////////
//	Called to display this applet as a property sheet
//
////////////////////////////////////////////////////////
BOOL CreatePropertySheet(HWND hwndParent, int iApplet)
{
	BOOL bReturn = FALSE;

	PROPSHEETHEADER psh;
	PROPSHEETPAGE ppsp;
	HPROPSHEETPAGE hpsp[2];

	int nPages=2;

	// Set all values for first property page
	ppsp.dwSize = sizeof(PROPSHEETPAGE)+sizeof(HWND); // Extra space at end of struct to tuck page's hwnd in when it's created
	ppsp.dwFlags = PSP_DEFAULT|PSP_USETITLE|PSP_USECALLBACK;
	ppsp.hInstance = g_hInstance;
	ppsp.pszTemplate = (LPTSTR)MAKEINTRESOURCE(IDD_PROPPAGE0);
	ppsp.hIcon = NULL;
	ppsp.pszTitle = TEXT("Screen Off");
	ppsp.pfnDlgProc = PowerModePageProc;
	ppsp.lParam = (LONG)ID_POWEROFFMODE;
	ppsp.pfnCallback = PropSheetPageProc;
	ppsp.pcRefParent = NULL;

	hpsp[0] = CreatePropertySheetPage(&ppsp);
	if (NULL == hpsp[0]) {
		//Fail out of function
		return bReturn;
	}

	// Set all values for second perperty page
	ppsp.dwSize = sizeof(PROPSHEETPAGE)+sizeof(HWND); // Extra space at end of struct to tuck page's hwnd in when it's created
	ppsp.dwFlags = PSP_DEFAULT|PSP_USETITLE|PSP_USECALLBACK;
	ppsp.hInstance = g_hInstance;
	ppsp.pszTemplate = (LPTSTR)MAKEINTRESOURCE(IDD_PROPPAGE1);
	ppsp.hIcon = NULL;
	ppsp.pszTitle = TEXT("Screen On");
	ppsp.pfnDlgProc = PowerModePageProc;
	ppsp.lParam = (LONG)ID_POWERONMODE;
	ppsp.pfnCallback = PropSheetPageProc;
	ppsp.pcRefParent = NULL;

	hpsp[1] = CreatePropertySheetPage(&ppsp);
	if (NULL ==hpsp[1]) {
		//Clean up the page we have created
		DestroyPropertySheetPage(hpsp[0]);

		//Fail out of function
		return bReturn;
	}
	
	psh.dwSize				= sizeof(psh);
	psh.dwFlags				= PSH_USECALLBACK|PSH_MAXIMIZE;
	psh.hwndParent			= hwndParent;
	psh.hInstance			= g_hInstance;
	psh.pszCaption			= NULL;
	psh.phpage				= hpsp;
	psh.nPages				= nPages;
	psh.nStartPage			= iApplet > (nPages-1)? (nPages-1): iApplet;
	psh.pfnCallback			= PropSheetProc;

	if(-1 != PropertySheet(&psh))
	{
		bReturn = TRUE;
	}
	else 
	{
		//Clean up PropertySheetPages as they weren't used by the PropertySheet
		DestroyPropertySheetPage(hpsp[0]);
		DestroyPropertySheetPage(hpsp[1]);
	}

	return bReturn;
}
