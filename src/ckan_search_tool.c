/*
 * ckan_search_tool.c
 *
 *  Created on: 26 Nov 2020
 *      Author: billy
 */


#include "ckan_search_tool.h"

#include "curl_tools.h"
#include "streams.h"
#include "byte_buffer.h"
#include "string_utils.h"
#include "key_value_pair.h"
#include "lucene_tool.h"


static json_t *GetResult (const json_t *ckan_result_p, LuceneTool *lucene_p, const SearchServiceData *data_p);

static json_t *ParseCKANResults (const json_t *ckan_results_p, LuceneTool *lucene_p, const SearchServiceData *data_p);


static bool ParseResultGroups (json_t *grassroots_result_p, const json_t *groups_p, LuceneTool *lucene_p, const SearchServiceData *data_p);


/*
 * https://ckan.grassroots.tools/api/3/action/package_search?q=Watkins&fq=groups:dfw-publications
 */


json_t *SearchCKAN (const char *query_s, LuceneTool *lucene_p, const SearchServiceData *data_p)
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
							if (AppendStringsToByteBuffer (buffer_p, data_p -> ssd_ckan_url_s, "/api/3/action/package_search?q=", escaped_query_s, NULL))
								{
									bool success_flag = true;

									/* we no longer need the escaped query so let's delete it */
									FreeURLEscapedString (escaped_query_s);

									if (data_p -> ssd_ckan_filters_p)
										{
											size_t i;
											json_t *filter_p;

											json_array_foreach (data_p -> ssd_ckan_filters_p, i, filter_p)
												{
													const char *key_s = GetJSONString (filter_p, "key");

													if (key_s)
														{
															const char *value_s = GetJSONString (filter_p, "value");

															if (value_s)
																{
																	if (!AppendStringsToByteBuffer (buffer_p, "&fq=", key_s, ":", value_s, NULL))
																		{
																			success_flag = false;
																			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to append \"&fq=\". \"%s\", \":\", \"%s\" to byte buffer", key_s, value_s);
																		}
																}
															else
																{
																	PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, filter_p, "Failed to get key");
																	success_flag = false;
																}

														}
													else
														{
															PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, filter_p, "Failed to get value");
															success_flag = false;
														}

												}

										}		/* if (filters_p) */

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
																	json_t *ckan_results_p = json_loads (result_s, 0, &err);

																	if (ckan_results_p)
																		{
																			grassroots_results_p = ParseCKANResults (ckan_results_p, lucene_p, data_p);
																			json_decref (ckan_results_p);
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
													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to append \"%s\", \"/api/3/action/package_search?q=\" and \"%s\"", data_p -> ssd_ckan_url_s, query_s);
												}

										}		/* if (success_flag) */


								}		/* if (AppendStringsToByteBuffer (buffer_p, ckan_search_url_s, "?q=", query_s, NULL)) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to append \"%s\", \"/api/3/action/package_search?q=\" and \"%s\"", data_p -> ssd_ckan_url_s, query_s);
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
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate ByteBuffer for CKAN");
				}

			FreeCurlTool (curl_p);
		}		/* if (curl_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate CurlTool for CKAN");
		}

	return grassroots_results_p;
}


static json_t *ParseCKANResults (const json_t *ckan_results_p, LuceneTool *lucene_p, const SearchServiceData *data_p)
{
	const json_t *ckan_result_p = json_object_get (ckan_results_p, "result");

	if (ckan_result_p)
		{
			const json_t *results_p = json_object_get (ckan_result_p, "results");
			json_int_t count = -1;

			GetJSONInteger (ckan_result_p, "count", &count);

			if (results_p)
				{
					if (json_is_array (results_p))
						{
							json_t *res_p = json_object ();

							if (res_p)
								{
									json_t *grassroots_results_p = json_array ();

									if (grassroots_results_p)
										{

											size_t i;

											json_array_foreach (results_p, i, ckan_result_p)
												{
													json_t *grassroots_result_p = GetResult (ckan_result_p, lucene_p, data_p);

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
															PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, ckan_result_p, "Failed to create grassroots result");
														}
												}

											return grassroots_results_p;
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
							PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, results_p, "Failed to get results is not an array");
						}
				}
			else
				{
					PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, ckan_result_p, "Failed to get results");
				}

		}
	else
		{
			PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, ckan_results_p, "Failed to get result");
		}

	return NULL;
}




static json_t *GetResult (const json_t *ckan_result_p, LuceneTool *lucene_p, const SearchServiceData *data_p)
{
	json_t *grassroots_result_p = NULL;
	const char *id_s = GetJSONString (ckan_result_p, "id");

	if (id_s)
		{
			char *url_s = ConcatenateVarargsStrings (data_p -> ssd_ckan_url_s, "/dataset/", id_s, NULL);

			if (url_s)
				{
					const char *title_s = GetJSONString (ckan_result_p, "title");

					if (title_s)
						{
							grassroots_result_p = json_object ();

							if (grassroots_result_p)
								{
									bool success_flag = false;

													if (SetJSONString (grassroots_result_p, LUCENE_ID_S, url_s))
														{
															if (SetJSONString (grassroots_result_p, WEB_SERVICE_URL_S, url_s))
																{
																	if (SetJSONString (grassroots_result_p, INDEXING_NAME_S, title_s))
																		{
																			json_t *groups_p = json_object_get (ckan_result_p, "groups");
																			const char *value_s = GetJSONString (ckan_result_p, "notes");

																			if (value_s)
																				{
																					if (!SetJSONString (grassroots_result_p, INDEXING_DESCRIPTION_S, value_s))
																						{
																							PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", INDEXING_DESCRIPTION_S, value_s);
																						}
																				}

																			value_s = GetJSONString (ckan_result_p, "author");
																			if (value_s)
																				{
																					/*
																					 * Is it a json object as some ckan plugins
																					 * can return json.
																					 */
																					json_error_t err;
																					json_t *authors_p = json_loads (value_s, 0, &err);
																					bool set_authors_flag = false;
																					const char * const AUTHORS_KEY_S = "author";

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

																													set_authors_flag = SetJSONString (grassroots_result_p, AUTHORS_KEY_S, authors_s);

																													if (!set_authors_flag)
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

																							json_decref (authors_p);
																						}		/* if (authors_p) */

																					/*
																					 * If we didn't manage to set it from a json object
																					 * just set it as a string
																					 */
																					if (!set_authors_flag)
																						{
																							if (!SetJSONString (grassroots_result_p, AUTHORS_KEY_S, value_s))
																								{
																									PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", AUTHORS_KEY_S, value_s);
																								}
																						}
																				}
																			else
																				{
																					PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "No authors specified");
																				}

																			if (data_p -> ssd_ckan_result_icon_s)
																				{
																					if (!SetJSONString (grassroots_result_p, INDEXING_ICON_URI_S, data_p -> ssd_ckan_result_icon_s))
																						{
																							PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", INDEXING_ICON_URI_S, data_p -> ssd_ckan_result_icon_s);
																						}
																				}

																			if (data_p -> ssd_ckan_provider_p)
																				{
																					if (json_object_set (grassroots_result_p, SERVER_PROVIDER_S, data_p -> ssd_ckan_provider_p) != 0)
																						{
																							PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\" object", SERVER_PROVIDER_S);
																						}
																				}


																			if (groups_p)
																				{
																					if (ParseResultGroups (grassroots_result_p, groups_p, lucene_p, data_p))
																						{
																							PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, groups_p, "ParseResultGroups () failed");
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
																	PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", WEB_SERVICE_URL_S, url_s);
																}
														}
													else
														{
															PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, grassroots_result_p, "Failed to set \"%s\": \"%s\"", LUCENE_ID_S, url_s);
														}

									if (!success_flag)
										{
											json_decref (grassroots_result_p);
											grassroots_result_p = NULL;
										}
								}
							else
								{
									PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "Failed to allocate json for grassroots result");
								}

						}
					else
						{
							PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, ckan_result_p, "no title key");
						}

				}
			else
				{
					PrintErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, "ConcatenateVarargsStrings failed for \"%s\", \"/dataset/\", \"%s\"", data_p -> ssd_ckan_url_s, id_s);
				}

		}
	else
		{
			PrintJSONToErrors (STM_LEVEL_WARNING, __FILE__, __LINE__, ckan_result_p, "no id key");
		}

	return grassroots_result_p;
}


static bool ParseResultGroups (json_t *grassroots_result_p, const json_t *groups_p, LuceneTool *lucene_p, const SearchServiceData *data_p)
{
	size_t i;
	const json_t *group_p;
	bool success_flag = true;

	json_array_foreach (groups_p, i, group_p)
		{
			const char *type_s = GetJSONString (group_p, "title");

			if (data_p -> ssd_ckan_resource_mappings_p)
				{
					const json_t *resource_p = json_object_get (data_p -> ssd_ckan_resource_mappings_p, type_s);

					if (resource_p)
						{
							const char *indexing_type_s = GetJSONString (resource_p, INDEXING_TYPE_S);
							const char *datatype_description_s = GetJSONString (resource_p, INDEXING_DESCRIPTION_S);
							const char *image_s = GetJSONString (resource_p, INDEXING_ICON_URI_S);


							if (SetJSONString (grassroots_result_p, INDEXING_TYPE_S, indexing_type_s))
								{
									if (SetJSONString (grassroots_result_p, INDEXING_TYPE_DESCRIPTION_S, datatype_description_s))
										{
											if (!AddFacetResultToLucene (lucene_p, datatype_description_s, 1))
												{
													const uint32 count = 1;

													PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to add \"%s\": " UINT32_FMT " as lucene facet", indexing_type_s, count);
												}

										}

								}

						}
					else
						{
							PrintJSONToErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, group_p, "Unknown type \"%s\"", type_s);
						}

				}


		}

	return success_flag;
}
