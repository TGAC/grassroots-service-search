/*
 * zenodo_search_tool.c
 *
 *  Created on: 26 Nov 2020
 *      Author: billy
 */


#include "zenodo_search_tool.h"

#include "curl_tools.h"
#include "streams.h"
#include "byte_buffer.h"
#include "string_utils.h"
#include "key_value_pair.h"
#include "lucene_tool.h"


static json_t *GetResult (const json_t *zenodo_result_p, json_t *facet_counts_p, const SearchServiceData *data_p);

static json_t *ParseZenodoResults (const json_t *zenodo_results_p, const SearchServiceData *data_p);

static bool ParseResultGroups (const json_t *groups_p, json_t *facet_counts_p);


/*
 * https://zenodo.grassroots.tools/api/3/action/package_search?q=Watkins&fq=groups:dfw-publications
 */


json_t *SearchZenodo (const char *query_s, const SearchServiceData *data_p)
{
	json_t *grassroots_results_p = NULL;
	CurlTool *curl_p = AllocateMemoryCurlTool (0);

	if (curl_p)
		{
			ByteBuffer *buffer_p = AllocateByteBuffer (1024);

			if (buffer_p)
				{
					char *escaped_query_s = GetURLEscapedString (curl_p, query_s);

					if (escaped_query_s)
						{
							if (AppendStringsToByteBuffer (buffer_p, data_p -> ssd_zenodo_url_s, "/api/records?access_token=", data_p -> ssd_zenodo_api_token_s, "&q=", escaped_query_s, NULL))
								{
									bool success_flag = true;

									if (data_p -> ssd_zenodo_community_s)
										{
											if (! (AppendStringsToByteBuffer (buffer_p, "&communities=", data_p -> ssd_zenodo_community_s, NULL)))
												{
													success_flag = false;
												}
										}


									/* we no longer need the escaped query so let's delete it */
									FreeURLEscapedString (escaped_query_s);

									if (success_flag)
										{
											const char *url_s = GetByteBufferData (buffer_p);

											if (SetUriForCurlTool (curl_p, url_s))
												{
													CURLcode res = RunCurlTool (curl_p);

													if (res == CURLE_OK)
														{
															const char *result_s = GetCurlToolData (curl_p);

															if (result_s)
																{
																	json_error_t err;
																	json_t *zenodo_results_p = json_loads (result_s, 0, &err);

																	if (zenodo_results_p)
																		{
																			grassroots_results_p = ParseZenodoResults (zenodo_results_p, data_p);
																			json_decref (zenodo_results_p);
																		}
																	else
																		{
																			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "json_loads () failed for url \"%s\" with error at %d,%d\n\"%s\"\n", url_s, err.line, err.column, err.text, result_s);
																		}

																}		/* if (result_s) */
															else
																{
																	PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "GetCurlToolData () failed for \"%s\"", url_s);
																}

														}		/* if (res == CURLE_OK) */
													else
														{
															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "RunCurlTool () Failed for \"%s\" with error code %d", url_s, res);
														}

												}		/* if (SetUriForCurlTool (curl_p, url_s)) */
											else
												{
													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to append \"%s\", \"/api/3/action/package_search?q=\" and \"%s\"", data_p -> ssd_zenodo_url_s, query_s);
												}

										}		/* if (success_flag) */


								}		/* if (AppendStringsToByteBuffer (buffer_p, zenodo_search_url_s, "?q=", query_s, NULL)) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to append \"%s\", \"/api/3/action/package_search?q=\" and \"%s\"", data_p -> ssd_zenodo_url_s, query_s);
								}

						}		/* if (escaped_query_s) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to get escaped query for \"%s\"", query_s);
						}


					FreeByteBuffer (buffer_p);
				}		/* if (buffer_p) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate ByteBuffer for Zenodo");
				}

			FreeCurlTool (curl_p);
		}		/* if (curl_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate CurlTool for Zenodo");
		}

	return grassroots_results_p;
}


static json_t *ParseZenodoResults (const json_t *zenodo_results_p, const SearchServiceData *data_p)
{
	const json_t *zenodo_hits_data_p = json_object_get (zenodo_results_p, "hits");

	if (zenodo_hits_data_p)
		{
			const json_t *hits_p = json_object_get (zenodo_hits_data_p, "results");
			json_int_t total = -1;

			GetJSONInteger (zenodo_hits_data_p, "total", &total);

			if (hits_p)
				{
					if (json_is_array (hits_p))
						{
							json_t *res_p = json_object ();

							if (res_p)
								{
									json_t *grassroots_results_p = json_array ();

									if (grassroots_results_p)
										{
											if (json_object_set_new (res_p, "results", grassroots_results_p) == 0)
												{
													json_t *facet_counts_p = json_object ();

													if (facet_counts_p)
														{
															if (json_object_set_new (res_p, "facets", facet_counts_p) == 0)
																{
																	size_t i;
																	const json_t *zenodo_hit_p;

																	json_array_foreach (hits_p, i, zenodo_hit_p)
																		{
																			json_t *grassroots_result_p = GetResult (zenodo_hit_p, facet_counts_p, data_p);

																			if (grassroots_result_p)
																				{
																					if (json_array_append_new (grassroots_results_p, grassroots_result_p) != 0)
																						{
																							PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, grassroots_result_p, "Failed to add grassroots result");
																							json_decref (grassroots_result_p);
																						}
																				}
																			else
																				{
																					PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, zenodo_hit_p, "Failed to create grassroots result");
																				}
																		}

																	return res_p;

																}		/* if (json_object_set_new (res_p, "facets", facet_counts_p) == 0) */
															else
																{
																	PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, res_p, "Failed to add facet counts");
																	json_decref (facet_counts_p);
																}

														}		/* if (facet_counts_p) */
													else
														{
															PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create facet counts");
														}
												}
											else
												{
													PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, res_p, "Failed to add results");
													json_decref (grassroots_results_p);
												}
										}
									else
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create grassroots results array");
										}

								}		/* if (res_p) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create json object ()");
								}

						}
					else
						{
							PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, hits_p, "Failed to get results is not an array");
						}
				}
			else
				{
					PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, hits_p, "Failed to get results");
				}

		}
	else
		{
			PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, zenodo_results_p, "Failed to get hits data");
		}

	return NULL;
}




static json_t *GetResult (const json_t *zenodo_result_p, json_t *facet_counts_p, const SearchServiceData *data_p)
{
	json_t *grassroots_result_p = NULL;
	const char *doi_url_s = GetJSONString (zenodo_result_p, "doi");

	if (doi_url_s)
		{
			const char *title_s = GetJSONString (zenodo_result_p, "title");

			if (title_s)
				{
					grassroots_result_p = json_object ();

					if (grassroots_result_p)
						{
							bool success_flag = false;
							const json_t *metadata_p = json_object_get (zenodo_result_p, "metadata");

							if (metadata_p)
								{
									const json_t *resource_type_p = json_object_get (metadata_p, "resource_type");
									const char *description_s = GetJSONString (metadata_p, "description");
									const char *indexing_type_s = NULL;
									const char *datatype_description_s = NULL;
									const char *image_s = NULL;

									if (description_s)
										{
											if (!SetJSONString (grassroots_result_p, INDEXING_DESCRIPTION_S, description_s))
												{
													PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", INDEXING_DESCRIPTION_S, description_s);
												}
										}

									if (resource_type_p)
										{
											/*
												 The different resource types are:

											    publication
											    poster
											    presentation
											    dataset
											    image
											    video
											    software
											    lesson
											    other
											*/

											const char *type_s = GetJSONString (resource_type_p, "type");

											if (type_s)
												{
													if (data_p -> ssd_zenodo_resource_mappings_p)
														{
															const json_t *resource_p = json_object_get (data_p -> ssd_zenodo_resource_mappings_p, type_s);

															if (resource_p)
																{
																	indexing_type_s = GetJSONString (resource_p, INDEXING_TYPE_S);
																	datatype_description_s = GetJSONString (resource_p, INDEXING_TYPE_DESCRIPTION_S);
																	image_s = GetJSONString (resource_p, INDEXING_ICON_URI_S);
																}

														}

												}

										}



									if (SetJSONString (grassroots_result_p, INDEXING_TYPE_S, indexing_type_s))
										{
											if (SetJSONString (grassroots_result_p, INDEXING_TYPE_DESCRIPTION_S, datatype_description_s))
												{
													if (SetJSONString (grassroots_result_p, LUCENE_ID_S, doi_url_s))
														{
															if (SetJSONString (grassroots_result_p, WEB_SERVICE_URL_S, doi_url_s))
																{
																	if (SetJSONString (grassroots_result_p, INDEXING_NAME_S, title_s))
																		{
																			json_t *authors_p = json_object_get (zenodo_result_p, "creators");

																			if (authors_p)
																				{
																					if (json_is_array (authors_p))
																						{
																							ByteBuffer *buffer_p = AllocateByteBuffer (1024);

																							if (buffer_p)
																								{
																									size_t num_authors = json_array_size (authors_p);
																									size_t i = 0;
																									bool loop_flag = true;
																									bool authors_success_flag = true;
																									bool first_entry_flag = true;
																									const char * const AUTHORS_KEY_S = "author";

																									while (loop_flag && authors_success_flag)
																										{
																											json_t *entry_p = json_array_get (authors_p, i);

																											if (json_is_object (entry_p))
																												{
																													const char *name_s = GetJSONString (entry_p, "name");

																													if (name_s)
																														{
																															if (first_entry_flag)
																																{
																																	authors_success_flag = AppendStringToByteBuffer (buffer_p, name_s);
																																	first_entry_flag = false;
																																}
																															else
																																{
																																	authors_success_flag = AppendStringsToByteBuffer (buffer_p, "; ", name_s, NULL);
																																}

																															if (!authors_success_flag)
																																{
																																	PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to append \"%s\" to authors", name_s);
																																}
																														}
																												}

																											if (authors_success_flag)
																												{
																													++ i;

																													if (i == num_authors)
																														{
																															loop_flag = false;
																														}
																												}
																										}		/* while (loop_flag && success_flag */

																									if (authors_success_flag)
																										{
																											const char *authors_s = GetByteBufferData (buffer_p);

																											if (!SetJSONString (grassroots_result_p, AUTHORS_KEY_S, authors_s))
																												{
																													PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to set authors to \"%s\"", authors_s);
																												}
																										}

																									FreeByteBuffer (buffer_p);
																								}

																						}		/* if (json_is_array (authors_p)) */
																					else
																						{
																							PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, authors_p, "Authors is not a JSON array");
																						}

																				}		/* if (authors_p) */
																			else
																				{
																					PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "No authors specified");
																				}

																			if (image_s)
																				{
																					if (!SetJSONString (grassroots_result_p, INDEXING_ICON_URI_S, image_s))
																						{
																							PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", INDEXING_ICON_URI_S, image_s);
																						}
																				}

																			success_flag = true;
																		}
																	else
																		{
																			PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", INDEXING_NAME_S, title_s);
																		}

																}
															else
																{
																	PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", WEB_SERVICE_URL_S, doi_url_s);
																}
														}
													else
														{
															PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", LUCENE_ID_S, doi_url_s);
														}
												}
											else
												{
													PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", INDEXING_TYPE_DESCRIPTION_S, datatype_description_s);
												}
										}
									else
										{
											PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", INDEXING_TYPE_S, indexing_type_s);
										}

									if (!success_flag)
										{
											json_decref (grassroots_result_p);
											grassroots_result_p = NULL;
										}
								}		/* if (metadata_p) */
							else
								{
									PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, zenodo_result_p, "no metadata");
								}
						}
					else
						{
							PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to allocate json for grassroots result");
						}

				}		/* if (title_s) */
			else
				{
					PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, zenodo_result_p, "no title key");
				}

		}		/* if (doi_url_s) */
	else
		{
			PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, zenodo_result_p, "no id key");
		}

	return grassroots_result_p;
}


static bool ParseResultGroups (const json_t *groups_p, json_t *facet_counts_p)
{
	size_t i;
	const json_t *group_p;
	bool success_flag = true;

	json_array_foreach (groups_p, i, group_p)
		{
			const char *facet_s = GetJSONString (group_p, "title");

			if (facet_s)
				{
					json_int_t count;

					if (GetJSONInteger (facet_counts_p, facet_s, &count))
						{
							++ count;
						}
					else
						{
							count = 1;
						}

					if (!SetJSONInteger (facet_counts_p, facet_s, count))
						{
							success_flag = false;
						}
				}
		}

	return success_flag;
}
