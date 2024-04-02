/*
 * zenodo_search_tool.h
 *
 *  Created on: 25 Mar 2024
 *      Author: billy
 */

#ifndef SERVICES_SEARCH_SERVICE_INCLUDE_ZENODO_SEARCH_TOOL_H_
#define SERVICES_SEARCH_SERVICE_INCLUDE_ZENODO_SEARCH_TOOL_H_

#include "search_service_data.h"
#include "search_service_library.h"


#ifdef __cplusplus
extern "C"
{
#endif


SEARCH_SERVICE_LOCAL json_t *SearchZenodo (const char *query_s, json_t *facet_counts_p, const SearchServiceData *data_p);


#ifdef __cplusplus
}
#endif


#endif /* SERVICES_SEARCH_SERVICE_INCLUDE_ZENODO_SEARCH_TOOL_H_ */
