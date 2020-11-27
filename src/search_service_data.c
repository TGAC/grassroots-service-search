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


bool ConfigureSearchServiceData (SearchServiceData *data_p)
{
	bool success_flag = false;

	const json_t *search_service_config_p = data_p -> ssd_base_data.sd_config_p;

	if (search_service_config_p)
		{
			data_p -> ssd_ckan_url_s = GetJSONString (search_service_config_p, "ckan_url");

			if (data_p -> ssd_ckan_url_s)
				{
					const json_t *mapping_p = json_object_get (search_service_config_p, "ckan_datatype");
					data_p -> ssd_ckan_filters_p = json_object_get (search_service_config_p, "ckan_filters");

					if (mapping_p)
						{
							data_p -> ssd_ckan_type_s = GetJSONString (mapping_p, INDEXING_TYPE_S);
							data_p -> ssd_ckan_type_description_s = GetJSONString (mapping_p, INDEXING_TYPE_DESCRIPTION_S);
						}

					success_flag = true;
				}

		}		/* if (search_service_config_p) */


	return success_flag;
}
