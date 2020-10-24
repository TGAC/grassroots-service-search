#include <string.h>
#include "search_service_data.h"

#include "memory_allocations.h"


SearchServiceData *AllocateSearchServiceData (void)
{
	SearchServiceData *data_p = AllocMemory (sizeof (SearchServiceData));

	if (data_p)
		{
			memset (data_p, 0, sizeof (SearchServiceData));
		}

	return data_p;
}


void FreeSearchServiceData (SearchServiceData *data_p)
{
	FreeMemory (data_p);
}
