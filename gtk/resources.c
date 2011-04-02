/* XPMs */
#include "cc.xpm"

#define RESOURCE_IDS_ONLY

/* Associated IDs */
#include "cc.h"

#define RESOURCE_MAX 1

long _resource_id[RESOURCE_MAX] = {
	MAIN_FRAME
};

char *_resource_data[RESOURCE_MAX] = {
	(char *)cc_xpm
};

typedef struct _resource_struct {
	long resource_max, *resource_id;
	char **resource_data;
} DWResources;

DWResources _resources = { RESOURCE_MAX, _resource_id, _resource_data };
