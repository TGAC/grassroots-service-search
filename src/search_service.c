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
 * search_service.c
 *
 *  Created on: 21 Sep 2020
 *      Author: billy
 */

#include "search_service.h"

#include "unsigned_int_parameter.h"
#include "string_parameter.h"

#include "audit.h"
#include "streams.h"
#include "math_utils.h"
#include "string_utils.h"

#include "lucene_tool.h"
#include "key_value_pair.h"
#include "mongodb_tool.h"

/*
 * Static declarations
 */


static NamedParameterType S_KEYWORD = { "SS Keyword Search", PT_KEYWORD };
static NamedParameterType S_FACET = { "SS Facet", PT_STRING };
static NamedParameterType S_PAGE_NUMBER = { "SS Results Page Number", PT_UNSIGNED_INT };
static NamedParameterType S_PAGE_SIZE = { "SS Results Page Size", PT_UNSIGNED_INT };

static const char * const S_ANY_FACET_S = "<ANY>";


static const uint32 S_DEFAULT_PAGE_NUMBER = 0;
static const uint32 S_DEFAULT_PAGE_SIZE = 500;


static Service *GetSearchService (GrassrootsServer *grassroots_p);

static const char *GetSearchServiceName (const Service *service_p);

static const char *GetSearchServiceDescription (const Service *service_p);

static const char *GetSearchServiceAlias (const Service *service_p);

static const char *GetSearchServiceInformationUri (const Service *service_p);

static ParameterSet *GetSearchServiceParameters (Service *service_p, Resource *resource_p, UserDetails *user_p);

static bool GetSearchServiceParameterTypesForNamedParameters (const Service *service_p, const char *param_name_s, ParameterType *pt_p);


static void ReleaseSearchServiceParameters (Service *service_p, ParameterSet *params_p);

static ServiceJobSet *RunSearchService (Service *service_p, ParameterSet *param_set_p, UserDetails *user_p, ProvidersStateTable *providers_p);

static ParameterSet *IsResourceForSearchService (Service *service_p, Resource *resource_p, Handler *handler_p);

static bool CloseSearchService (Service *service_p);

static ServiceMetadata *GetSearchServiceMetadata (Service *service_p);


static void SearchKeyword (const char *keyword_s, const char *facet_s, const uint32 page_number, const uint32 page_size, ServiceJob *job_p, SearchServiceData *data_p);


static bool AddResultsFromLuceneResults (LuceneDocument *document_p, const uint32 index, void *data_p);

static Parameter *AddFacetParameter (ParameterSet *params_p, ParameterGroup *group_p, SearchServiceData *data_p);


typedef struct
{
	SearchServiceData *sd_service_data_p;
	ServiceJob *sd_job_p;
} SearchData;



/*
 * API definitions
 */


ServicesArray *GetServices (UserDetails *user_p, GrassrootsServer *grassroots_p)
{
	Service *service_p = GetSearchService (grassroots_p);

	if (service_p)
		{
			/*
			 * Since we only have a single Service, create a ServicesArray with
			 * 1 item.
			 */
			ServicesArray *services_p = AllocateServicesArray (1);

			if (services_p)
				{
					* (services_p -> sa_services_pp) = service_p;


					return services_p;
				}

			FreeService (service_p);
		}

	return NULL;
}


void ReleaseServices (ServicesArray *services_p)
{
	FreeServicesArray (services_p);
}



static Service *GetSearchService (GrassrootsServer *grassroots_p)
{
	Service *service_p = (Service *) AllocMemory (sizeof (Service));

	if (service_p)
		{
			SearchServiceData *data_p = AllocateSearchServiceData ();

			if (data_p)
				{
					if (InitialiseService (service_p,
														 GetSearchServiceName,
														 GetSearchServiceDescription,
														 GetSearchServiceAlias,
														 GetSearchServiceInformationUri,
														 RunSearchService,
														 IsResourceForSearchService,
														 GetSearchServiceParameters,
														 GetSearchServiceParameterTypesForNamedParameters,
														 ReleaseSearchServiceParameters,
														 CloseSearchService,
														 NULL,
														 false,
														 SY_SYNCHRONOUS,
														 (ServiceData *) data_p,
														 GetSearchServiceMetadata,
														 NULL,
														 grassroots_p))
						{
							return service_p;
						}		/* if (InitialiseService (.... */

					FreeSearchServiceData (data_p);
				}

			FreeMemory (service_p);
		}		/* if (service_p) */

	return NULL;
}



static const char *GetSearchServiceName (const Service * UNUSED_PARAM (service_p))
{
	return "Search Grassroots";
}


static const char *GetSearchServiceDescription (const Service * UNUSED_PARAM (service_p))
{
	return "A service to search across all of the Grassroots data and tools";
}


static const char *GetSearchServiceAlias (const Service * UNUSED_PARAM (service_p))
{
	return "search";
}


static const char *GetSearchServiceInformationUri (const Service *service_p)
{
	const char *url_s = GetServiceInformationPage (service_p);

	if (!url_s)
		{
			url_s = "https://grassroots.tools/docs/user/services/search/;
		}

	return url_s;
}


static Parameter *AddFacetParameter (ParameterSet *params_p, ParameterGroup *group_p, SearchServiceData *data_p)
{
	StringParameter *param_p = (StringParameter *) EasyCreateAndAddStringParameterToParameterSet (& (data_p -> ssd_base_data), params_p, group_p, S_FACET.npt_type, S_FACET.npt_name_s, "Type", "The type of data to search for", S_ANY_FACET_S, PL_ALL);

	if (param_p)
		{
			if (CreateAndAddStringParameterOption (param_p, S_ANY_FACET_S, "Any"))
				{
					if (data_p -> ssd_base_data.sd_config_p)
						{
							json_t *facets_p = json_object_get (data_p -> ssd_base_data.sd_config_p, "facets");

							if (facets_p)
								{
									if (json_is_array (facets_p))
										{
											size_t i;
											json_t *facet_p;

											json_array_foreach (facets_p, i, facet_p)
												{
													const char *name_s = GetJSONString (facet_p, "so:name");

													if (name_s)
														{
															const char *description_s = GetJSONString (facet_p, "so:description");

															if (name_s)
																{
																	if (!CreateAndAddStringParameterOption (param_p, name_s, description_s))
																		{
																			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add parameter option \"%s\": \"%s\"", name_s, description_s);
																		}
																}

														}

												}		/* json_array_foreach (facets_p, i, facet_p) */

										}		/* if (json_is_array (facets_p)) */

								}		/* if (facets_p) */

						}		/* if (data_p -> ssd_base_data.sd_config_p) */

					return & (param_p -> sp_base_param);
				}
		}

	return NULL;
}


static ParameterSet *GetSearchServiceParameters (Service *service_p, Resource * UNUSED_PARAM (resource_p), UserDetails * UNUSED_PARAM (user_p))
{
	ParameterSet *params_p = AllocateParameterSet ("search service parameters", "The parameters used for the  search service");

	if (params_p)
		{
			SearchServiceData *data_p = (SearchServiceData *) service_p -> se_data_p;
			ParameterGroup *group_p = NULL;
			Parameter *param_p = NULL;

			if ((param_p = EasyCreateAndAddStringParameterToParameterSet (& (data_p -> ssd_base_data), params_p, group_p, S_KEYWORD.npt_type, S_KEYWORD.npt_name_s, "Search", "Search the field trial data", NULL, PL_ALL)) != NULL)
				{
					if (AddFacetParameter (params_p, group_p, data_p))
						{
							uint32 def = S_DEFAULT_PAGE_NUMBER;

							if ((param_p = EasyCreateAndAddUnsignedIntParameterToParameterSet (& (data_p -> ssd_base_data), params_p, group_p, S_PAGE_NUMBER.npt_name_s, "Page", "The number of the results page to get", &def, PL_ADVANCED)) != NULL)
								{
									def = S_DEFAULT_PAGE_SIZE;

									if ((param_p = EasyCreateAndAddUnsignedIntParameterToParameterSet (& (data_p -> ssd_base_data), params_p, group_p, S_PAGE_SIZE.npt_name_s, "Page size", "The maximum number of results on each page", &def, PL_ADVANCED)) != NULL)
										{
											return params_p;
										}		/* if ((param_p = EasyCreateAndAddParameterToParameterSet (& (data_p -> ssd_base_data), params_p, group_p, S_PAGE_SIZE.npt_type, S_PAGE_SIZE.npt_name_s, "Page size", "The maximum number of results on each page", def, PL_SIMPLE)) != NULL) */
									else
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add %s parameter", S_PAGE_SIZE.npt_name_s);
										}

								}		/* if ((param_p = EasyCreateAndAddParameterToParameterSet (& (data_p -> ssd_base_data), params_p, group_p, S_PAGE_NUMBER.npt_type, S_PAGE_NUMBER.npt_name_s, "Page", "The page of results to get", def, PL_SIMPLE)) != NULL) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add %s parameter", S_PAGE_NUMBER.npt_name_s);
								}

						}		/* if (AddFacetParameter (params_p, group_p, data_p)) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add Facet parameter");
						}

				}		/* if ((param_p = EasyCreateAndAddParameterToParameterSet (data_p, params_p, NULL, S_KEYWORD.npt_type, S_KEYWORD.npt_name_s, "Search", "Search the field trial data", def, PL_SIMPLE)) != NULL) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add %s parameter", S_KEYWORD.npt_name_s);
				}


			FreeParameterSet (params_p);
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate %s ParameterSet", GetSearchServiceName (service_p));
		}

	return NULL;
}


static bool GetSearchServiceParameterTypesForNamedParameters (const Service *service_p, const char *param_name_s, ParameterType *pt_p)
{
	bool success_flag = true;

	if (strcmp (param_name_s, S_KEYWORD.npt_name_s) == 0)
		{
			*pt_p = S_KEYWORD.npt_type;
		}
	else if (strcmp (param_name_s, S_FACET.npt_name_s) == 0)
		{
			*pt_p = S_FACET.npt_type;
		}
	else if (strcmp (param_name_s, S_PAGE_NUMBER.npt_name_s) == 0)
		{
			*pt_p = S_PAGE_NUMBER.npt_type;
		}
	else if (strcmp (param_name_s, S_PAGE_SIZE.npt_name_s) == 0)
		{
			*pt_p = S_PAGE_SIZE.npt_type;
		}
	else
		{
			success_flag = false;
		}

	return success_flag;
}



static void ReleaseSearchServiceParameters (Service * UNUSED_PARAM (service_p), ParameterSet *params_p)
{
	FreeParameterSet (params_p);
}


static bool CloseSearchService (Service *service_p)
{
	bool success_flag = true;

	FreeSearchServiceData ((SearchServiceData *) (service_p -> se_data_p));

	return success_flag;
}


static ServiceJobSet *RunSearchService (Service *service_p, ParameterSet *param_set_p, UserDetails * UNUSED_PARAM (user_p), ProvidersStateTable * UNUSED_PARAM (providers_p))
{
	SearchServiceData *data_p = (SearchServiceData *) (service_p -> se_data_p);

	service_p -> se_jobs_p = AllocateSimpleServiceJobSet (service_p, NULL, "");

	if (service_p -> se_jobs_p)
		{
			ServiceJob *job_p = GetServiceJobFromServiceJobSet (service_p -> se_jobs_p, 0);

			LogParameterSet (param_set_p, job_p);

			SetServiceJobStatus (job_p, OS_FAILED_TO_START);

			if (param_set_p)
				{
					const char *keyword_s = NULL;
					const char *facet_s = NULL;
					const uint32 *page_number_p = NULL;
					const uint32 *page_size_p = NULL;

					GetCurrentStringParameterValueFromParameterSet (param_set_p, S_KEYWORD.npt_name_s, &keyword_s);
					GetCurrentStringParameterValueFromParameterSet (param_set_p, S_FACET.npt_name_s, &facet_s);

					if (facet_s)
						{
							if (strcmp (facet_s, S_ANY_FACET_S) == 0)
								{
									facet_s = NULL;
								}
						}

					GetCurrentUnsignedIntParameterValueFromParameterSet (param_set_p, S_PAGE_NUMBER.npt_name_s, &page_number_p);
					GetCurrentUnsignedIntParameterValueFromParameterSet (param_set_p, S_PAGE_SIZE.npt_name_s, &page_size_p);


					SearchKeyword (keyword_s, facet_s, page_number_p ? *page_number_p : S_DEFAULT_PAGE_NUMBER, page_size_p ? *page_size_p : S_DEFAULT_PAGE_SIZE, job_p, data_p);
				}		/* if (param_set_p) */

#if DFW_FIELD_TRIAL_SERVICE_DEBUG >= STM_LEVEL_FINE
			PrintJSONToLog (STM_LEVEL_FINE, __FILE__, __LINE__, job_p -> sj_metadata_p, "metadata 3: ");
#endif

			LogServiceJob (job_p);
		}		/* if (service_p -> se_jobs_p) */

	return service_p -> se_jobs_p;
}


static ServiceMetadata *GetSearchServiceMetadata (Service * UNUSED_PARAM (service_p))
{
	const char *term_url_s = CONTEXT_PREFIX_EDAM_ONTOLOGY_S "topic_0625";
	SchemaTerm *category_p = AllocateSchemaTerm (term_url_s, "Genotype and phenotype",
		"The study of genetic constitution of a living entity, such as an individual, and organism, a cell and so on, "
		"typically with respect to a particular observable phenotypic traits, or resources concerning such traits, which "
		"might be an aspect of biochemistry, physiology, morphology, anatomy, development and so on.");

	if (category_p)
		{
			SchemaTerm *subcategory_p;

			term_url_s = CONTEXT_PREFIX_EDAM_ONTOLOGY_S "operation_0304";
			subcategory_p = AllocateSchemaTerm (term_url_s, "Query and retrieval", "Search or query a data resource and retrieve entries and / or annotation.");

			if (subcategory_p)
				{
					ServiceMetadata *metadata_p = AllocateServiceMetadata (category_p, subcategory_p);

					if (metadata_p)
						{
							SchemaTerm *input_p;

							term_url_s = CONTEXT_PREFIX_EDAM_ONTOLOGY_S "data_0968";
							input_p = AllocateSchemaTerm (term_url_s, "Keyword",
								"Boolean operators (AND, OR and NOT) and wildcard characters may be allowed. Keyword(s) or phrase(s) used (typically) for text-searching purposes.");

							if (input_p)
								{
									if (AddSchemaTermToServiceMetadataInput (metadata_p, input_p))
										{
											SchemaTerm *output_p;

											/* Place */
											term_url_s = CONTEXT_PREFIX_SCHEMA_ORG_S "Place";
											output_p = AllocateSchemaTerm (term_url_s, "Place", "Entities that have a somewhat fixed, physical extension.");

											if (output_p)
												{
													if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
														{
															/* Date */
															term_url_s = CONTEXT_PREFIX_SCHEMA_ORG_S "Date";
															output_p = AllocateSchemaTerm (term_url_s, "Date", "A date value in ISO 8601 date format.");

															if (output_p)
																{
																	if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																		{
																			/* Pathogen */
																			term_url_s = CONTEXT_PREFIX_EXPERIMENTAL_FACTOR_ONTOLOGY_S "EFO_0000643";
																			output_p = AllocateSchemaTerm (term_url_s, "pathogen", "A biological agent that causes disease or illness to its host.");

																			if (output_p)
																				{
																					if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																						{
																							/* Phenotype */
																							term_url_s = CONTEXT_PREFIX_EXPERIMENTAL_FACTOR_ONTOLOGY_S "EFO_0000651";
																							output_p = AllocateSchemaTerm (term_url_s, "phenotype", "The observable form taken by some character (or group of characters) "
																								"in an individual or an organism, excluding pathology and disease. The detectable outward manifestations of a specific genotype.");

																							if (output_p)
																								{
																									if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																										{
																											/* Genotype */
																											term_url_s = CONTEXT_PREFIX_EXPERIMENTAL_FACTOR_ONTOLOGY_S "EFO_0000513";
																											output_p = AllocateSchemaTerm (term_url_s, "genotype", "Information, making the distinction between the actual physical material "
																												"(e.g. a cell) and the information about the genetic content (genotype).");

																											if (output_p)
																												{
																													if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p))
																														{
																															return metadata_p;
																														}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																													else
																														{
																															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																															FreeSchemaTerm (output_p);
																														}

																												}		/* if (output_p) */
																											else
																												{
																													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																												}
																										}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																									else
																										{
																											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																											FreeSchemaTerm (output_p);
																										}

																								}		/* if (output_p) */
																							else
																								{
																									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																								}

																						}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																					else
																						{
																							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																							FreeSchemaTerm (output_p);
																						}

																				}		/* if (output_p) */
																			else
																				{
																					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																				}

																		}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
																	else
																		{
																			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
																			FreeSchemaTerm (output_p);
																		}

																}		/* if (output_p) */
															else
																{
																	PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
																}


														}		/* if (AddSchemaTermToServiceMetadataOutput (metadata_p, output_p)) */
													else
														{
															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add output term %s to service metadata", term_url_s);
															FreeSchemaTerm (output_p);
														}

												}		/* if (output_p) */
											else
												{
													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate output term %s for service metadata", term_url_s);
												}

										}		/* if (AddSchemaTermToServiceMetadataInput (metadata_p, input_p)) */
									else
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add input term %s to service metadata", term_url_s);
											FreeSchemaTerm (input_p);
										}

								}		/* if (input_p) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate input term %s for service metadata", term_url_s);
								}

						}		/* if (metadata_p) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate service metadata");
						}

				}		/* if (subcategory_p) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate sub-category term %s for service metadata", term_url_s);
				}

		}		/* if (category_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate category term %s for service metadata", term_url_s);
		}

	return NULL;
}


static ParameterSet *IsResourceForSearchService (Service * UNUSED_PARAM (service_p), Resource * UNUSED_PARAM (resource_p), Handler * UNUSED_PARAM (handler_p))
{
	return NULL;
}


static void SearchKeyword (const char *keyword_s, const char *facet_s, const uint32 page_number, const uint32 page_size, ServiceJob *job_p,  SearchServiceData *data_p)
{
	OperationStatus status = OS_FAILED_TO_START;
	GrassrootsServer *grassroots_p = GetGrassrootsServerFromService (data_p -> ssd_base_data.sd_service_p);
	LuceneTool *lucene_p = AllocateLuceneTool (grassroots_p, job_p -> sj_id);

	if (lucene_p)
		{
			bool success_flag = true;
			LinkedList *facets_p = NULL;

			if (facet_s)
				{
					facets_p = AllocateLinkedList (FreeKeyValuePairNode);

					if (facets_p)
						{
							KeyValuePairNode *facet_p = AllocateKeyValuePairNodeByParts (lucene_p -> lt_facet_key_s, facet_s);

							if (facet_p)
								{
									LinkedListAddTail (facets_p, & (facet_p -> kvpn_node));
								}		/* if (facet_p) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "AllocateKeyValuePairNode for facet \"type\": \"%s\" failed", facet_s);
									success_flag = false;
								}

						}		/* if (facets_p) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "AllocateLinkedList for facet \"%s\" failed", facet_s);
							success_flag = false;
						}

				}		/* if (facet_s) */


			if (success_flag)
				{
					if (SetLuceneToolName (lucene_p, "search_keywords"))
						{
							if (SearchLucene (lucene_p, keyword_s, facets_p, "drill-down", page_number, page_size))
								{
									SearchData sd;
									const uint32 from = page_number * page_size;
									const uint32 to = from + page_size - 1;

									sd.sd_service_data_p = data_p;
									sd.sd_job_p = job_p;

									status = ParseLuceneResults (lucene_p, from, to, AddResultsFromLuceneResults, &sd);

									if ((status == OS_SUCCEEDED) || (status == OS_PARTIALLY_SUCCEEDED))
										{
											bool added_metadata_flag = false;
											json_error_t error;
											json_t *metadata_p = json_pack_ex (&error, 0, "{s:i,s:i,s:i}",
																												 LT_NUM_TOTAL_HITS_S, lucene_p -> lt_num_total_hits,
																												 LT_HITS_START_INDEX_S, lucene_p -> lt_hits_from_index,
																												 LT_HITS_END_INDEX_S, lucene_p -> lt_hits_to_index);

											if (metadata_p)
												{
													if (AddLuceneFacetResultsToJSON (lucene_p, metadata_p))
														{
															added_metadata_flag = true;
														}

													job_p -> sj_metadata_p = metadata_p;
												}
											else
												{
													PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to create metadata for lucene hits");
												}

											if ((!added_metadata_flag) && (status == OS_SUCCEEDED))
												{
													status = OS_PARTIALLY_SUCCEEDED;
												}

										}		/* if ((status == OS_SUCCEEDED) || (status == OS_PARTIALLY_SUCCEEDED)) */
									else
										{
											const char *status_s = GetOperationStatusAsString (status);

											PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "ParseLuceneResults failed for \"%s\"", keyword_s);
										}

								}		/* if (SearchLucene (lucene_p, keyword_s, facets_p, "drill-down", page_number, page_size)) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "SearchLucene for \"%s\" failed", keyword_s);
								}

						}		/* if (SetLuceneToolName ("search_keywords")) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "SetLuceneToolName to \"search-keywords\" failed");
						}


					if (facets_p)
						{
							FreeLinkedList (facets_p);
						}

				}		/* if (success_flag) */
			else
				{
					PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to allocate facets list for \"%s\"", keyword_s);
				}

			FreeLuceneTool (lucene_p);
		}		/* if (lucene_p) */
	else
		{
			PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to allocate lucene tool for \"%s\"", keyword_s);
		}


	SetServiceJobStatus (job_p, status);
}


static bool AddResultsFromLuceneResults (LuceneDocument *document_p, const uint32 index, void *data_p)
{
	bool success_flag = false;
	SearchData *search_data_p = (SearchData *) data_p;
	const char *id_s = GetDocumentFieldValue (document_p, LUCENE_ID_S);

	if (id_s)
		{
			const char *type_s = GetDocumentFieldValue (document_p, "@type");

			if (type_s)
				{
					const char *name_s = GetDocumentFieldValue (document_p, "so:name");
					json_t *result_p = GetCopyOfDocuemnt (document_p);

					if (result_p)
						{
							if (strcmp (type_s, "Grassroots:Service") == 0)
								{
									const char * const PAYLOAD_KEY_S = "payload";
									const char *payload_s = GetJSONString (result_p, PAYLOAD_KEY_S);

									if (payload_s)
										{
											json_error_t err;
											json_t *payload_p = json_loads (payload_s, 0, &err);

											if (payload_p)
												{
													if (json_object_set_new (result_p, PAYLOAD_KEY_S, payload_p) != 0)
														{
															PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, result_p, "Failed to add unpacked payload");
															json_decref (payload_p);
														}
												}
											else
												{
													PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, result_p, "Failed to load payload \"%s\", %s", payload_s, err.text);
												}
										}
									else
										{
											PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, result_p, "No payload");
										}

								}

							json_t *dest_record_p = GetResourceAsJSONByParts (PROTOCOL_INLINE_S, NULL, name_s, result_p);

							if (dest_record_p)
								{
									if (AddResultToServiceJob (search_data_p -> sd_job_p, dest_record_p))
										{
											success_flag = true;
										}
									else
										{
											json_decref (dest_record_p);
										}

								}		/* if (dest_record_p) */

							json_decref (result_p);
						}		/* if (result_p) */

				}		/* if (type_s) */

		}		/* if (id_s) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get \"%s\" from document", LUCENE_ID_S);
		}

	return success_flag;
}

