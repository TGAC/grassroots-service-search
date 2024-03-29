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
			const json_t *ckan_p = json_object_get (search_service_config_p, "ckan");
			const json_t *zenodo_p = json_object_get (search_service_config_p, "zenodo");

			success_flag = true;

			if (ckan_p)
				{
					data_p -> ssd_ckan_url_s = GetJSONString (ckan_p, CONTEXT_PREFIX_SCHEMA_ORG_S "url");

					if (data_p -> ssd_ckan_url_s)
						{
							const json_t *mapping_p = json_object_get (ckan_p, "datatype");

							if (mapping_p)
								{
									data_p -> ssd_ckan_type_s = GetJSONString (mapping_p, INDEXING_TYPE_S);
									data_p -> ssd_ckan_type_description_s = GetJSONString (mapping_p, INDEXING_TYPE_DESCRIPTION_S);
									data_p -> ssd_ckan_result_icon_s = GetJSONString (mapping_p, INDEXING_ICON_URI_S);
								}

							data_p -> ssd_zenodo_provider_name_s = GetJSONString (ckan_p, CONTEXT_PREFIX_SCHEMA_ORG_S "name");
							data_p -> ssd_zenodo_provider_icon_s = GetJSONString (ckan_p, CONTEXT_PREFIX_SCHEMA_ORG_S "description");
							data_p -> ssd_ckan_filters_p = json_object_get (ckan_p, "filters");
						}
				}

			if (zenodo_p)
				{
					data_p -> ssd_zenodo_url_s = GetJSONString (zenodo_p, CONTEXT_PREFIX_SCHEMA_ORG_S "url");

					if (data_p -> ssd_zenodo_url_s)
						{
							data_p -> ssd_zenodo_provider_name_s = GetJSONString (zenodo_p, CONTEXT_PREFIX_SCHEMA_ORG_S "name");
							data_p -> ssd_zenodo_provider_icon_s = GetJSONString (zenodo_p, CONTEXT_PREFIX_SCHEMA_ORG_S "description");
							data_p -> ssd_zenodo_community_s = GetJSONString (zenodo_p, "community");
							data_p -> ssd_zenodo_api_token_s = GetJSONString (zenodo_p, "api_token");
						}
				}


		}		/* if (search_service_config_p) */


	return success_flag;
}
