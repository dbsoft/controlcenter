#include <windows.h>
#include <iphlpapi.h>
#include "dw.h"

#define SystemBasicInformation       0
#define SystemPerformanceInformation 2
#define SystemTimeInformation        3

#define Li2Double(x) ((double)((x).HighPart) * 4.294967296E9 + (double)((x).LowPart))

typedef struct
{
    DWORD   dwUnknown1;
    ULONG   uKeMaximumIncrement;
    ULONG   uPageSize;
    ULONG   uMmNumberOfPhysicalPages;
    ULONG   uMmLowestPhysicalPage;
    ULONG   uMmHighestPhysicalPage;
    ULONG   uAllocationGranularity;
    PVOID   pLowestUserAddress;
    PVOID   pMmHighestUserAddress;
    ULONG   uKeActiveProcessors;
    BYTE    bKeNumberProcessors;
    BYTE    bUnknown2;
    WORD    wUnknown3;
} SYSTEM_BASIC_INFORMATION;

typedef struct 
{
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
    ULONG ReadOperationCount;
    ULONG WriteOperationCount;
    ULONG OtherOperationCount;
    ULONG AvailablePages;
    ULONG TotalCommittedPages;
    ULONG TotalCommitLimit;
    ULONG PeakCommitment;
    ULONG PageFaults;           // total soft or hard Page Faults since boot (wraps at 32-bits)
    ULONG Reserved[74]; // unknown
} SYSTEM_PERFORMANCE_INFORMATION;

typedef struct
{
    LARGE_INTEGER liKeBootTime;
    LARGE_INTEGER liKeSystemTime;
    LARGE_INTEGER liExpTimeZoneBias;
    ULONG         uCurrentTimeZoneId;
    DWORD         dwReserved;
} SYSTEM_TIME_INFORMATION;


// ntdll!NtQuerySystemInformation (NT specific!)
//
// The function copies the system information of the
// specified type into a buffer
//
// NTSYSAPI
// NTSTATUS
// NTAPI
// NtQuerySystemInformation(
//    IN UINT SystemInformationClass,    // information type
//    OUT PVOID SystemInformation,       // pointer to buffer
//    IN ULONG SystemInformationLength,  // buffer size in bytes
//    OUT PULONG ReturnLength OPTIONAL   // pointer to a 32-bit
//                                       // variable that receives
//                                       // the number of bytes
//                                       // written to the buffer 
// );
typedef LONG (WINAPI *PROCNTQSI)(UINT,PVOID,ULONG,PULONG);

PROCNTQSI NtQuerySystemInformation = 0;

double NT_Load(void)
{
    static SYSTEM_PERFORMANCE_INFORMATION SysPerfInfo;
    static SYSTEM_TIME_INFORMATION        SysTimeInfo;
    static SYSTEM_BASIC_INFORMATION       SysBaseInfo;
    static double                         dbIdleTime;
    static double                         dbSystemTime;
    static LARGE_INTEGER                  liOldIdleTime = {0,0};
    static LARGE_INTEGER                  liOldSystemTime = {0,0};
    LONG                           status;

	if(!NtQuerySystemInformation)
		NtQuerySystemInformation = (PROCNTQSI)GetProcAddress(
															 GetModuleHandle("ntdll"),
															 "NtQuerySystemInformation"
															);

    if (!NtQuerySystemInformation)
        return 0;

    // get number of processors in the system
	status = NtQuerySystemInformation(SystemBasicInformation,&SysBaseInfo,sizeof(SysBaseInfo),NULL);
	if (status != NO_ERROR)
		return 0;

	// get new system time
	status = NtQuerySystemInformation(SystemTimeInformation,&SysTimeInfo,sizeof(SysTimeInfo),0);
	if (status!=NO_ERROR)
		return 0;

	// get new CPU's idle time
	status = NtQuerySystemInformation(SystemPerformanceInformation,&SysPerfInfo,sizeof(SysPerfInfo),NULL);
	if (status != NO_ERROR)
		return 0;

	// if it's a first call - skip it
	if (liOldIdleTime.QuadPart != 0)
	{
		// CurrentValue = NewValue - OldValue
		dbIdleTime = Li2Double(SysPerfInfo.IdleTime) - Li2Double(liOldIdleTime);
		dbSystemTime = Li2Double(SysTimeInfo.liKeSystemTime) - Li2Double(liOldSystemTime);

		// CurrentCpuIdle = IdleTime / SystemTime
		dbIdleTime = dbIdleTime / dbSystemTime;

		// CurrentCpuUsage% = 100 - (CurrentCpuIdle * 100) / NumberOfProcessors
		dbIdleTime = 100.0 - dbIdleTime * 100.0 / (double)SysBaseInfo.bKeNumberProcessors + 0.5;
	}

	// store new CPU's idle and system time
	liOldIdleTime = SysPerfInfo.IdleTime;
	liOldSystemTime = SysTimeInfo.liKeSystemTime;

	return dbIdleTime/100;
}


/* This should return true for WinNT/2K/XP and false on Win9x */
int IsWinNT(void)
{
	static int isnt = -1;

	if(isnt == -1)
	{
		if(GetVersion() < 0x80000000)
			isnt = 1;
		else
			isnt = 0;
	}
	return isnt;
}

int Get_Uptime(unsigned long *Seconds)
{
	LARGE_INTEGER counter, freq;

	if(QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&counter))
	{
		double secs = Li2Double(counter) / Li2Double(freq);
		*Seconds = (unsigned long)secs;
	}
	else
		*Seconds = (unsigned long)GetTickCount()/1000;
	return 0;
}


void StartCPUMonitor (void)
{
	HKEY hkey;
	DWORD size;
	DWORD data;
	DWORD type;


	if (RegOpenKeyEx(HKEY_DYN_DATA, "PerfStats\\StartStat", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
	{ size = sizeof(DWORD);
		RegQueryValueEx(hkey, "KERNEL\\CPUUsage", NULL, &type, (LPBYTE)&data, &size);
	RegCloseKey(hkey);
	}
}

int Get_Load(double *Load)
{
	*Load = 0;

	if(IsWinNT())
	{
		*Load = (double)NT_Load();
	}
	else
	{
		static int firsttime = 1;
		HKEY hkey;
		DWORD size;
		DWORD data;
		DWORD type;

		if(firsttime)
		{
			StartCPUMonitor();
			firsttime = 0;
		}

		if(RegOpenKeyEx(HKEY_DYN_DATA, "PerfStats\\StatData", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			size = sizeof(DWORD);
			if(RegQueryValueEx(hkey, "KERNEL\\CPUUsage", NULL, &type, (LPBYTE)&data, &size) == ERROR_SUCCESS)
				*Load = (double)data/100;
			RegCloseKey(hkey);
			return 0;
		}
	}
	return 1;
}


int Get_Memory(long double *Memory)
{
	MEMORYSTATUSEX ms;
	
	ms.dwLength = sizeof(ms);

	GlobalMemoryStatusEx(&ms);

	// Convert to kilobytes to prevent overflows on 64 bit systems
	*Memory = (long double)ms.ullAvailPhys;
	return 0;
}

unsigned long ifr_in[100], ifr_out[100];

int Get_Net(unsigned long *Sent, unsigned long *Recv, unsigned long *TotalSent, unsigned long *TotalRecv)
{
	ULONG size = sizeof(MIB_IFTABLE) + (sizeof(MIB_IFROW) * 100);
	MIB_IFTABLE *ift = malloc(size);
	static int first = 1;
	DWORD count;
	int z;

	*Sent = *Recv = *TotalSent = *TotalRecv = 0;

	if(first)
	{
		memset(ifr_in, 0, sizeof(unsigned long) * 100);
		memset(ifr_out, 0, sizeof(unsigned long) * 100);
	}

	if(GetIfTable(ift, &size, TRUE) == NO_ERROR)
	{
		for(z=0;z<ift->dwNumEntries;z++)
		{
			if(ift->table[z].dwType != MIB_IF_TYPE_OTHER && ift->table[z].dwType != MIB_IF_TYPE_LOOPBACK)
			{
				if(!first)
				{
					*Sent += ift->table[z].dwOutOctets - ifr_out[z];
					*Recv += ift->table[z].dwInOctets - ifr_in[z];
				}
				ifr_in[z] = ift->table[z].dwInOctets;
				*TotalRecv += ifr_in[z];
				ifr_out[z] = ift->table[z].dwOutOctets;
				*TotalSent += ifr_out[z];
			}
		}
	}

	free(ift);

	if(first)
		first = 0;

	return 0;
}
