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
} SearchServiceData;


SearchServiceData *AllocateSearchServiceData (void);

void FreeSearchServiceData (SearchServiceData *data_p);


#endif /* SEARCH_SERVICE_DATA_H */