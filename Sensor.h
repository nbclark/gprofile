#define SENSOR_DLL      L"HTCSensorSDK.dll"

#define SENSOR_TILT_PORT 1

typedef struct _SENSORDATA
{
    int Reserved0; // this value is always 3
	byte Luminance; // this value ranges between 0 and 30
} SENSORDATA, *PSENSORDATA;

typedef struct _SENSORDATATILT
{
    SHORT   TiltX;          // From -1000 to 1000 (about), 0 is flat
    SHORT   TiltY;          // From -1000 to 1000 (about), 0 is flat
    SHORT   Orientation;    // From -1000 to 1000 (about)
                            // 0 = Straight up, -1000 = Flat, 1000 = Upside down
    WORD    Unknown1;       // Always zero
    DWORD   AngleY;         // From 0 to 359
    DWORD   AngleX;         // From 0 to 359
    DWORD   Unknown2;       // Bit field?
} SENSORDATATILT, *PSENSORDATATILT;

typedef HANDLE (WINAPI * PFN_HTCSensorOpen)(DWORD);
typedef void (WINAPI * PFN_HTCSensorClose)(HANDLE);
typedef DWORD (WINAPI * PFN_HTCSensorGetDataOutput)(HANDLE, PSENSORDATA);
typedef DWORD (WINAPI * PFN_HTCSensorGetDataOutputTILT)(HANDLE, PSENSORDATATILT);
typedef DWORD (WINAPI * PFN_HTCSensorQueryCapability)(DWORD);

PFN_HTCSensorOpen           pfnHTCSensorOpen;
PFN_HTCSensorClose          pfnHTCSensorClose;
PFN_HTCSensorGetDataOutput  pfnHTCSensorGetDataOutput;
PFN_HTCSensorGetDataOutputTILT  pfnHTCSensorGetDataOutputTilt;
PFN_HTCSensorQueryCapability  pfnHTCSensorQueryCapability;

bool SensorInit(DWORD sensorValue, HANDLE* phSensor)
{
	*phSensor = NULL;

    HMODULE hSensorLib = LoadLibrary(SENSOR_DLL);

    if (hSensorLib == NULL)
    {
        return false;
    }

    pfnHTCSensorOpen = (PFN_HTCSensorOpen)
        GetProcAddress(hSensorLib, L"HTCSensorOpen");
    pfnHTCSensorClose = (PFN_HTCSensorClose)
        GetProcAddress(hSensorLib, L"HTCSensorClose");
    pfnHTCSensorGetDataOutput = (PFN_HTCSensorGetDataOutput)
        GetProcAddress(hSensorLib, L"HTCSensorGetDataOutput");
    pfnHTCSensorGetDataOutputTilt = (PFN_HTCSensorGetDataOutputTILT)
        GetProcAddress(hSensorLib, L"HTCSensorGetDataOutput");

    pfnHTCSensorQueryCapability = (PFN_HTCSensorQueryCapability)
        GetProcAddress(hSensorLib, L"HTCSensorQueryCapability");

    if (pfnHTCSensorOpen == NULL ||
        pfnHTCSensorClose == NULL ||
        pfnHTCSensorGetDataOutput == NULL)
    {
        return false;
    }

    *phSensor = pfnHTCSensorOpen(sensorValue);

    return true;
}

bool SensorUninit(HANDLE hSensor)
{
    pfnHTCSensorClose(hSensor);

    return true;
}

void SensorGetData(HANDLE hSensor, SENSORDATA* pSensorData)
{
    pfnHTCSensorGetDataOutput(hSensor, pSensorData);
}

void SensorGetDataTilt(HANDLE hSensor, SENSORDATATILT* pSensorData)
{
    pfnHTCSensorGetDataOutputTilt(hSensor, pSensorData);
}