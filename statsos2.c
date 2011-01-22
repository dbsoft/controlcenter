#define TCPV40HDRS  1
#define BSD_SELECT
#define INCL_DOS
#include <os2.h>
#include <time.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <types.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include "si2.h"

APIRET16 APIENTRY16 Dos16MemAvail(PULONG pulAvailMem);
int sock = -1;
struct ifmib one;
unsigned long ulTotalIn[IFMIB_ENTRIES];
unsigned long ulTotalOut[IFMIB_ENTRIES];
unsigned long ulEstIn[IFMIB_ENTRIES];
unsigned long ulEstOut[IFMIB_ENTRIES];

typedef struct _uptime {
	double fifteenmin, fivemin, onemin;
	int done;
} Uptime;

int Get_Uptime(unsigned long *Seconds)
{
	QWORD qwTmrTime;
	APIRET rc;
	ULONG ulSeconds = 0;
	static ULONG ulTmrFreq = 0;

	if(!ulTmrFreq)
	{
		rc = DosTmrQueryFreq(&ulTmrFreq);
		if(rc)
		{
			ULONG ulMsCount;
			rc = DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, (PVOID)&ulMsCount, sizeof(ULONG));

			if(rc)
				return rc;

			ulSeconds = ulMsCount /1000;
		}
	}
	if(!ulSeconds)
	{
		rc = DosTmrQueryTime(&qwTmrTime);

		if(rc)
			return rc;

		ulSeconds  = LL2F(qwTmrTime.ulHi, qwTmrTime.ulLo) / ((double)ulTmrFreq);
	}
	*Seconds = ulSeconds;
	return 0;
}

int Get_Load(double *Load)
{
	static double  ts_val,   ts_val_prev[CPUS]  ={0.,0.,0.,0.};
	static double  idle_val, idle_val_prev[CPUS]={0.,0.,0.,0.};
	static double  busy_val, busy_val_prev[CPUS]={0.,0.,0.,0.};
	static double  intr_val, intr_val_prev[CPUS]={0.,0.,0.,0.};
	static CPUUTIL CPUUtil[CPUS];
	static int     CPUIdle[CPUS];
	static double  CPULoad[CPUS];
	static int iter = 0;
	int i;
	APIRET rc;

	do
	{
		rc = DosPerfSysCall(CMD_KI_RDCNT,(ULONG) &CPUUtil[0],0,0);
		if (rc)
			return rc;

        for(i=0; i < CPUS; i++)
		{
			ts_val   = LL2F(CPUUtil[i].ulTimeHigh, CPUUtil[i].ulTimeLow);
			idle_val = LL2F(CPUUtil[i].ulIdleHigh, CPUUtil[i].ulIdleLow);
			busy_val = LL2F(CPUUtil[i].ulBusyHigh, CPUUtil[i].ulBusyLow);
			intr_val = LL2F(CPUUtil[i].ulIntrHigh, CPUUtil[i].ulIntrLow);

			if(!ts_val)
				continue;

			if(iter > 0)
            {
				double ts_delta = ts_val - ts_val_prev[i];
				double usage = ((busy_val - busy_val_prev[i])/ts_delta);
				double pidle = ((idle_val - idle_val_prev[i])/ts_delta)*100.0;

				if(usage > 99.0)
					usage = 100;

				CPULoad[i] = usage;
				CPUIdle[i] = pidle;
			}

			ts_val_prev[i]   = ts_val;
            idle_val_prev[i] = idle_val;
			busy_val_prev[i] = busy_val;
            intr_val_prev[i] = intr_val;
		}
		if(!iter)
			DosSleep(32);

		if(iter < 3)
			iter++;
    }
	while(iter < 2);

	*Load = CPULoad[0];
	return 0;
}

int Get_Memory(unsigned long *Memory)
{
	Dos16MemAvail(Memory);
	return 0;
}

int Get_Net(unsigned long *Sent, unsigned long *Recv, unsigned long *TotalSent, unsigned long *TotalRecv)
{
	int i;
	static int firsttime = 1;

	*TotalRecv = *TotalSent = *Recv = *Sent = 0;

	if(sock < 1)
		sock = socket(PF_INET, SOCK_STREAM, 0);

	if(sock > 0)
	{
		ioctl(sock, SIOSTATIF, (caddr_t)&one, sizeof(one));

		for(i = 0; i < IFMIB_ENTRIES; i++)
		{
			if(!firsttime)
				*Recv += one.iftable[i].ifInOctets - ulTotalIn[i];
			ulTotalIn[i]  = one.iftable[i].ifInOctets;
			*TotalRecv += ulTotalIn[i];
			if(!firsttime)
				*Sent += one.iftable[i].ifOutOctets - ulTotalOut[i];
			ulTotalOut[i] = one.iftable[i].ifOutOctets;
			*TotalSent += ulTotalOut[i];
			ulEstIn[i] = ulEstOut[i] = 0;
		}

		if(firsttime)
			firsttime = 0;
	}
}
