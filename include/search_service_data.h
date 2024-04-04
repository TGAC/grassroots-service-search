/*
** Copyright 2014-2018 The Earlham Institute
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*
 * search_service_data.h
 *
 *  Created on: 24 Sep 2020
 *      Author: billy
 */

#ifndef SEARCH_SERVICE_DATA_H
#define SEARCH_SERVICE_DATA_H



#include "service.h"
#include "search_service_library.h"



typedef struct SearchServiceData
{
	ServiceData ssd_base_data;
	const char *ssd_ckan_url_s;
	const json_t *ssd_ckan_filters_p;
	const json_t *ssd_ckan_resource_mappings_p;
	const char *ssd_ckan_result_icon_s;
	const json_t *ssd_ckan_provider_p;

	const char *ssd_zenodo_url_s;
	const char *ssd_zenodo_community_s;
	const char *ssd_zenodo_api_token_s;
	const json_t *ssd_zenodo_resource_mappings_p;
	const json_t *ssd_zenodo_provider_p;

} SearchServiceData;


#ifdef __cplusplus
extern "C"
{
#endif



SEARCH_SERVICE_LOCAL SearchServiceData *AllocateSearchServiceData (void);

SEARCH_SERVICE_LOCAL void FreeSearchServiceData (SearchServiceData *data_p);

SEARCH_SERVICE_LOCAL bool ConfigureSearchServiceData (SearchServiceData *data_p);


#ifdef __cplusplus
}
#endif


#endif /* SEARCH_SERVICE_DATA_H */
