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
 * search_service.h
 *
 *  Created on: 24 Oct 2018
 *      Author: billy
 */

#ifndef SERVICES_FIELD_TRIALS_INCLUDE_SEARCH_SERVICE_H_
#define SERVICES_FIELD_TRIALS_INCLUDE_SEARCH_SERVICE_H_



#include "search_service_data.h"
#include "search_service_library.h"



#ifdef __cplusplus
extern "C"
{
#endif


SEARCH_SERVICE_API ServicesArray *GetServices (User *user_p, GrassrootsServer *grassroots_p);

SEARCH_SERVICE_API void ReleaseServices (ServicesArray *services_p);



#ifdef __cplusplus
}
#endif



#endif /* SERVICES_FIELD_TRIALS_INCLUDE_SEARCH_SERVICE_H_ */
