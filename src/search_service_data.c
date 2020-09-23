#include "search_service_data.h"

#include "memory_allocations.h"


SearchServiceData *AllocateSearchServiceData (void)
{
	return AllocMemory (sizeof (SearchServiceData));
}

void FreeSearchServiceData (SearchServiceData *data_p)
{
	FreeMemory (data_p);
}
