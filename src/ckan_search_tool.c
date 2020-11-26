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


static json_t *GetResult (const json_t *ckan_result_p, const char *ckan_url_s, json_t *facet_counts_p);

static json_t *ParseCKANResults (const json_t *ckan_results_p, const char *ckan_url_s);

static bool ParseResultGroups (const json_t *groups_p, json_t *facet_counts_p);


/*
 * https://ckan.grassroots.tools/api/3/action/package_search?q=Watkins&fq=groups:dfw-publications
 */


json_t *SearchCKAN (const char *ckan_url_s, const char *query_s, const json_t *filters_p)
{
	json_t *grassroots_results_p = NULL;
	CurlTool *curl_p = AllocateCurlTool (CM_MEMORY);

	if (curl_p)
		{
			ByteBuffer *buffer_p = AllocateByteBuffer (1024);

			if (buffer_p)
				{
					if (AppendStringsToByteBuffer (buffer_p, ckan_url_s, "/api/3/action/package_search?q=", query_s, NULL))
						{
							bool success_flag = true;

							if (filters_p)
								{
									size_t i;
									json_t *filter_p;

									json_array_foreach (filters_p, i, filter_p)
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
															}
													}
												else
													{
														success_flag = false;
													}

											}
										else
											{
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
																	grassroots_results_p = ParseCKANResults (ckan_results_p, ckan_url_s);
																	json_decref (ckan_results_p);
																}

														}		/* if (result_s) */

												}		/* if (res == CURLE_OK) */

										}		/* if (SetUriForCurlTool (curl_p, url_s)) */

								}		/* if (success_flag) */


						}		/* if (AppendStringsToByteBuffer (buffer_p, ckan_search_url_s, "?q=", query_s, NULL)) */

					FreeByteBuffer (buffer_p);
				}		/* if (buffer_p) */

			FreeCurlTool (curl_p);
		}		/* if (curl_p) */

	return grassroots_results_p;
}


static json_t *ParseCKANResults (const json_t *ckan_results_p, const char *ckan_url_s)
{
	const json_t *ckan_result_p = json_object_get (ckan_results_p, "result");

	if (ckan_result_p)
		{
			const json_t *results_p = json_object_get (ckan_result_p, "results");
			int count = -1;

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
											if (json_object_set_new (res_p, "results", grassroots_results_p) == 0)
												{
													json_t *facet_counts_p = json_object ();

													if (facet_counts_p)
														{
															if (json_object_set_new (res_p, "facets", facet_counts_p) == 0)
																{
																	size_t i;

																	json_array_foreach (results_p, i, ckan_result_p)
																	{
																		json_t *grassroots_result_p = GetResult (ckan_result_p, ckan_url_s, facet_counts_p);

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

																	return res_p;

																}		/* if (json_object_set_new (res_p, "facets", facet_counts_p) == 0) */
															else
																{
																	json_decref (facet_counts_p);
																}

														}		/* if (facet_counts_p) */
												}
											else
												{
													json_decref (grassroots_results_p);
												}
										}
									else
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create grassroots results array");
										}

								}		/* if (res_p) */


						}
				}

		}

	return NULL;
}




static json_t *GetResult (const json_t *ckan_result_p, const char *ckan_url_s, json_t *facet_counts_p)
{
	json_t *grassroots_result_p = NULL;
	const char *id_s = GetJSONString (ckan_result_p, "id");

	if (id_s)
		{
			char *url_s = ConcatenateStrings (ckan_url_s, id_s);

			if (url_s)
				{
					const char *title_s = GetJSONString (ckan_result_p, "title");

					if (title_s)
						{
							grassroots_result_p = json_object ();

							if (grassroots_result_p)
								{
									bool success_flag = false;

									if (SetJSONString (grassroots_result_p, INDEXING_TYPE_S, "Grassroots:Publication"))
										{
											if (SetJSONString (grassroots_result_p, INDEXING_TYPE_DESCRIPTION_S, "Grassroots:Publication"))
												{
													if (SetJSONString (grassroots_result_p, LUCENE_ID_S, url_s))
														{
															if (SetJSONString (grassroots_result_p, WEB_SERVICE_URL_S, url_s))
																{
																	if (SetJSONString (grassroots_result_p, INDEXING_NAME_S, title_s))
																		{
																			json_t *groups_p = json_object_get (ckan_result_p, "groups");
																			const char *notes_s = GetJSONString (ckan_result_p, "notes");

																			if (notes_s)
																				{
																					SetJSONString (grassroots_result_p, INDEXING_DESCRIPTION_S, notes_s);
																				}

																			if (groups_p)
																				{
																					ParseResultGroups (groups_p, facet_counts_p);
																				}

																			success_flag = true;
																		}

																}
														}
												}
										}

									if (!success_flag)
										{
											json_decref (grassroots_result_p);
											grassroots_result_p = NULL;
										}
								}

						}

				}
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
					int count;

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
