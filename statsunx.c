#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
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
#include <time.h>
#include <fcntl.h>
#if defined(__MAC__)
#include <mach/mach.h>
#include <mach/mach_error.h>
#endif
#if defined(__FreeBSD__) || defined(__MAC__)
#include <net/if_types.h>
#include <net/if_mib.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/vmmeter.h>
#include <unistd.h>
#include <math.h>
#endif
#ifdef __linux__
#include <linux/kernel.h>

struct sysinfo si;
#endif
int sock = -1;
unsigned long ulTotalIn[16];
unsigned long ulTotalOut[16];
unsigned long ulEstIn[16];
unsigned long ulEstOut[16];

int Get_Uptime(unsigned long *Seconds)
{
#ifdef __linux__
	sysinfo(&si);
	*Seconds = si.uptime;
#elif defined(__FreeBSD__) || defined(__MAC__)
	struct timeval boottime;
	size_t size = sizeof(boottime);
	time_t now;
	int mib[2] = { CTL_KERN, KERN_BOOTTIME };

	time(&now);
	if(sysctl(mib, 2, &boottime, &size, NULL, 0) > -1 && boottime.tv_sec != 0)
		*Seconds = now - boottime.tv_sec;
#else
	*Seconds = 0;
#endif
	return 0;
}

int Get_Load(double *Load)
{
#ifdef __linux__
	FILE *fp = fopen("/proc/stat", "r");
	static int lastused = 0, lasttotal = 0;
	static double lastload = 0;

	*Load =0;

	if(fp)
	{
		char buf[1024];
		int user, nice, sys, idle, used, total;

		fgets(buf, 1024, fp);

		sscanf(buf, "cpu %d %d %d %d", &user, &nice, &sys, &idle);

		used = (user+nice+sys);
		total = used + idle;

		if(lasttotal && (total - lasttotal))
			lastload = *Load = (double)(used-lastused)/(total-lasttotal);
		else
			*Load = lastload;

		lastused = used;
		lasttotal = total;

		fclose(fp);
	}
#elif defined(__FreeBSD__)
	static long lastused = 0, lasttotal = 0;
	static double lastload = 0;
	long counters[5], used, total;
	size_t len;
	
	/* Query the free and inactive pages */
	len = sizeof(counters);
	sysctlbyname("kern.cp_time", &counters, &len, 0, 0);

	/* Add up user, nice, system and interrupt to get total used */
	used = counters[0] + counters[1] + counters[2] + counters[3];

	/* Total adds the idle */
	total = used + counters[4];

	if(lasttotal && (total - lasttotal))
		lastload = *Load = (double)(used-lastused)/(total-lasttotal);
	else
		*Load = lastload;

	lastused = used;
	lasttotal = total;
#elif defined(__MAC__)
	static long lastused = 0, lasttotal = 0;
	static double lastload = 0;
	double used = 0, total = 0;
	natural_t cpuCount;
	processor_info_array_t infoArray;
	mach_msg_type_number_t infoCount;

	if(!host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpuCount, &infoArray, &infoCount))
	{
		int cpu;
		
		processor_cpu_load_info_data_t* cpuLoadInfo = (processor_cpu_load_info_data_t*) infoArray;
			
		for(cpu=0;cpu<cpuCount;cpu++)
		{
			double thisused = cpuLoadInfo[cpu].cpu_ticks[0] + cpuLoadInfo[cpu].cpu_ticks[1] + cpuLoadInfo[cpu].cpu_ticks[3];
			used += thisused;
			total += thisused + cpuLoadInfo[cpu].cpu_ticks[2];
		}
	}

	vm_deallocate(mach_task_self(), (vm_address_t)infoArray, infoCount);
	
	if(lasttotal && (total - lasttotal))
		lastload = *Load = (double)(used-lastused)/(total-lasttotal);
	else
		*Load = lastload;

	lastused = used;
	lasttotal = total;
#else
	*Load = 0;
#endif
	return 0;
}

int Get_Memory(long double *Memory)
{
#ifdef __linux__
	sysinfo(&si);
	/* Recent versions of Linux require multiplying by the memory unit */
	*Memory = (long double)(si.freeram * si.mem_unit);
#elif defined(__FreeBSD__) || defined(__MAC__)
	int pages_free, pages_inactive = 0;
	size_t len;
	
	/* Query the free and inactive pages */
#if defined(__MAC__)
	len = sizeof(pages_free);
	sysctlbyname("hw.usermem", &pages_free, &len, 0, 0);
	pages_free = pages_free / getpagesize();
#else
	len = sizeof(pages_free);
	sysctlbyname("vm.stats.vm.v_free_count", &pages_free, &len, 0, 0);
	len = sizeof(pages_inactive);
	sysctlbyname("vm.stats.vm.v_inactive_count", &pages_inactive, &len, 0, 0);
#endif

	/* Add them together and multiply by the page size to get the total free */
	*Memory = (long double)(pages_free + pages_inactive) * getpagesize();
#else
	*Memory = 0;
#endif
	return 0;
}

int Get_Net(unsigned long *Sent, unsigned long *Recv, unsigned long *TotalSent, unsigned long *TotalRecv)
{
#ifdef __linux__
	FILE *fp = fopen("/proc/net/dev", "r");
	static int firsttime = 1; 

	*TotalRecv = *TotalSent = *Recv = *Sent = 0;

	if(fp)
	{
		char buf[1024];
		int if_unit = 0;

		while(!feof(fp))
		{
			char *tmp;
			char *ifacename;
			int packets, bytes, errs, drop, fifo, frame, compressed, multicast, sbytes, serrs;

			fgets(buf, 1024, fp);

			tmp = buf;

			/* Find the first interface entry */
			while(*tmp == ' ')
				tmp++;
			/* Save the start of the name */
			ifacename = tmp;
			/* Find the end of the interface name */
			while(*tmp && *tmp != ':')
				tmp++;

			/* Looks like we found something! */
			if(*tmp == ':' && if_unit < 16)
			{
				/* Terminate the interface name */
				*tmp = 0;

				tmp++;

				sscanf(tmp, "%d %d %d %d %d %d %d %d %d %d", &bytes, &packets, &errs, &drop, &fifo, &frame,
					   &compressed, &multicast, &sbytes, &serrs);

				if(!firsttime)
				{
					*Recv += bytes - ulTotalIn[if_unit];
					*Sent += sbytes - ulTotalOut[if_unit];
				}
				ulTotalOut[if_unit] = sbytes;
				*TotalSent += sbytes;
				ulTotalIn[if_unit] = bytes;
				*TotalRecv += bytes;
				if_unit++;
			}
		}
		firsttime = 0;
		fclose(fp);
	}
#elif defined(__FreeBSD__) || defined(__MAC__)
	static int firsttime = 1;
	struct ifmibdata ifmd;

	*TotalRecv = *TotalSent = *Recv = *Sent = 0;

	if(sock < 1)
		sock = socket(PF_INET, SOCK_STREAM, 0);

	if(sock > 0)
	{
		static unsigned long lastin = 0, lastout = 0;
		int name[6], z, maxifno;
        unsigned long currentin = 0, currentout = 0;
		size_t len = sizeof(maxifno);

		name[0] = CTL_NET;
		name[1] = PF_LINK;
		name[2] = NETLINK_GENERIC;
		name[3] = IFMIB_SYSTEM;
		name[4] = IFMIB_IFCOUNT;

		if(sysctl(name, 5, &maxifno, &len, 0, 0) < 0)
			return 0;

		name[3] = IFMIB_IFDATA;
		name[5] = IFDATA_GENERAL;

		for(z=1;z<=maxifno;z++)
		{
			name[4] = z;
			len = sizeof(ifmd);

			if(sysctl(name, 6, &ifmd, &len, 0, 0) >-1)
			{
				currentin += ifmd.ifmd_data.ifi_ibytes;
				currentout += ifmd.ifmd_data.ifi_obytes;
			}
		}

		if(!firsttime)
			*Recv += currentin - lastin;
		lastin  = currentin;
		*TotalRecv += lastin;
		if(!firsttime)
			*Sent += currentout - lastout;
		lastout = currentout;
		*TotalSent += lastout;

		if(firsttime)
			firsttime = 0;
	}
#else
	*TotalRecv = *TotalSent = *Recv = *Sent = 0;
#endif
	return 0;
}
