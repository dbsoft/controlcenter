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
#ifdef __FreeBSD__
#include <net/if_types.h>
#include <net/if_mib.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/dkstat.h>
#include <sys/vmmeter.h>
#include <kvm.h>
#include <nlist.h>
#include <math.h>
#endif
#ifdef __linux__
#include <linux/kernel.h>
#include <linux/sys.h>

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
#elif defined(__FreeBSD__)
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

#ifdef __linux__
int lastused = 0, lasttotal = 0;
double lastload = 0;
#endif

#ifdef __FreeBSD__
struct nlist namelist[] = {
#define X_CPTIME        0
	{ "_cp_time" },
#define X_CNT           1
	{ "_cnt" },
	{""}
};
char *nlistf = NULL, *memf = NULL;

kvm_t	*kd = 0;

static int pageshift;		/* log base 2 of the pagesize */
#define pagetok(size) ((size) << pageshift)
#define LOG1024 10

void kernel_init(void)
{
    register int pagesize;
	int c;

	/* get the page size with "getpagesize" and calculate pageshift from it */
	pagesize = getpagesize();
	pageshift = 0;
	while (pagesize > 1)
	{
		pageshift++;
		pagesize >>= 1;
	}

	if ((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, "kvm_open")) == NULL)
		return;

    (void) kvm_nlist(kd, namelist);
	if (namelist[0].n_type == 0)
	{
		fprintf(stderr, "cc: nlist failed\n");
		return;
	}

    /* we only need the amount of log(2)1024 for our conversion */
    pageshift -= LOG1024;
}


/*
 * kread reads something from the kernel, given its nlist index.
 */
void kread(int nlx, void *addr, size_t size)
{
	char *sym;

	if (namelist[nlx].n_type == 0 || namelist[nlx].n_value == 0) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		fprintf(stderr, "symbol %s not defined", sym);
	}
	if (kvm_read(kd, namelist[nlx].n_value, addr, size) != size) {
		sym = namelist[nlx].n_name;
		if (*sym == '_')
			++sym;
		fprintf(stderr, "%s: %s", sym, kvm_geterr(kd));
	}
}
double cpu_load(void)
{
	register int state;
	double pct, total;
	long cp_time[CPUSTATES], idle;
	static long lastcp_time[CPUSTATES];
	static int firsttime = 1;

	if(!kd)
		return 0;

	if(firsttime)
	{
		kread(X_CPTIME, lastcp_time, sizeof(lastcp_time));
		firsttime = 0;
		return;
	}
	kread(X_CPTIME, cp_time, sizeof(cp_time));

	total = 0;
	for (state = 0; state < CPUSTATES; ++state)
		total += cp_time[state] - lastcp_time[state];

	idle = cp_time[CP_IDLE] - lastcp_time[CP_IDLE];

	memcpy(lastcp_time, cp_time, sizeof(lastcp_time));

	if(total)
	{
		pct = (double)(total - idle) / total;
		return pct;
	}
	return 0;
}

unsigned long free_memory(void)
{
	struct vmmeter sum;

	if(!kd)
		return 0;

	kread(X_CNT, &sum, sizeof(sum));

	return pagetok(sum.v_free_count);
}
#endif

int Get_Load(double *Load)
{
#ifdef __linux__
	FILE *fp = fopen("/proc/stat", "r");

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
	if(!kd)
		kernel_init();
	*Load = cpu_load();
#else
	*Load = 0;
#endif
	return 0;
}

int Get_Memory(unsigned long *Memory)
{
#ifdef __linux__
	sysinfo(&si);
	*Memory = si.freeram;
#elif defined(__FreeBSD__)
	if(!kd)
		kernel_init();
	*Memory = free_memory() * 1024;
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
			int packets, bytes, errs, drop, fifo, frame, compressed, multicast, sbytes, serrs;

			fgets(buf, 1024, fp);

			tmp = buf;

			/* Find the first interface entery */
			while(*tmp == ' ')
				tmp++;
			while(*tmp && *tmp != ':')
				tmp++;

			if(*tmp == ':' && if_unit < 16)
			{
				*tmp = 0;

				tmp++;

				sscanf(tmp, "%d %d %d %d %d %d %d %d %d", &bytes, &packets, &errs, &drop, &fifo, &frame,
					   &compressed, &multicast, &sbytes, &serrs);

				if(!firsttime)
				{
					*Recv = bytes - ulTotalIn[if_unit];
					*Sent = sbytes - ulTotalOut[if_unit];
				}
				ulTotalOut[if_unit] = *TotalSent = sbytes;
				ulTotalIn[if_unit] = *TotalRecv = bytes;
				if_unit++;
			}
		}
		firsttime = 0;
		fclose(fp);
	}
#elif defined(__FreeBSD__)
	int i;
	static int firsttime = 1;
	struct ifmibdata ifmd;

	*TotalRecv = *TotalSent = *Recv = *Sent = 0;

	if(sock < 1)
		sock = socket(PF_INET, SOCK_STREAM, 0);

	if(sock > 0)
	{
		static int lastin = 0, lastout = 0;
		int name[6], z, maxifno, currentin = 0, currentout = 0;
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
