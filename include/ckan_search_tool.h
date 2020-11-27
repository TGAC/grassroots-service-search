/*
 * ckan_search_tool.h
 *
 *  Created on: 26 Nov 2020
 *      Author: billy
 */

#ifndef SERVICES_SEARCH_SERVICE_INCLUDE_CKAN_SEARCH_TOOL_H_
#define SERVICES_SEARCH_SERVICE_INCLUDE_CKAN_SEARCH_TOOL_H_

#include "search_service_data.h"
#include "search_service_library.h"


#include "linked_list.h"



#ifdef __cplusplus
extern "C"
{
#endif


SEARCH_SERVICE_LOCAL json_t *SearchCKAN (const char *query_s, const SearchServiceData *data_p);


#ifdef __cplusplus
}
#endif


#endif /* SERVICES_SEARCH_SERVICE_INCLUDE_CKAN_SEARCH_TOOL_H_ */
