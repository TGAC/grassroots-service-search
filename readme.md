 
# Search Services {#search_service_guide}

The search service uses the [Grassroots Lucene library](https://github.com/TGAC/grassroots-lucene.git) to allow searches
of all of the data and services within a Grassroots server. It also allows searching of external datasets and other data management systems such as [CKAN](https://ckan.org/).

## Installation

To build this service, you need the [grassroots core](https://github.com/TGAC/grassroots-core) and [grassroots build config](https://github.com/TGAC/grassroots-build-config) installed and configured. 

The files to build the search service are in the ```build/<platform>``` directory. 

### Linux

If you enter this directory 

~~~
cd build/linux
~~~

you can then build the service by typing

~~~
make all
~~~

and then 

~~~
make install
~~~

to install the service into the Grassroots system where it will be available for use immediately.

## Service Configuration

Each of the three services listed above can be configured by files with the same names in the ```config``` directory in the Grassroots application directory, *e.g.* ```config/Search Grassroots```

 * **facets**: This is an array of objects giving the details of the available databases. The objects in this array have the following keys:
    * **so:name**:  This is the name to show to the user for this database. 
    * **so:description**: This is a user-friendly description to display to the user.

### CKAN configuration

 * **ckan_url**: If this is set, it specifies a CKAN instance to also search on.
 * **ckan_filters**: If this is set, it specifies a CKAN instance to also search on.
    * **key**:  This is the name to show to the user for this database. 
    * **description**: This is a user-friendly description to display to the user.
 * **ckan_datatype**: If this is set, it specifies a CKAN instance to also search on.
    * **@type**:  This is the name to show to the user for this database. 
    * **type_description**: This is a user-friendly description to display to the user.
    * **so:image**: This is a boolean value that specifies whether the database is selected to search against by default. 
An example configuration file for the search service which would be saved as the ```<Grassroots directory>/config/Search Grassroots``` is:

~~~{.json}
{
	"so:image": "http://localhost:2000/grassroots/images/search",
	"facets": [{
		"so:name": "Dataset",
		"so:description": "Dataset"
	}, {
		"so:name": "Service",
		"so:description": "Service"
	}, {
		"so:name": "Field Trial",
		"so:description": "Field Trial"
	}, {
		"so:name": "Study",
		"so:description": "Study"
	}, {
		"so:name": "Location",
		"so:description": "Location"
	}, {
		"so:name": "Measured Variable",
		"so:description": "Measured Variable"
	}, {
		"so:name": "Programme",
		"so:description": "Programme"
	}, {
		"so:name": "Publication",
		"so:description": "Publication"
	}],
	"ckan_url": "https://ckan.grassroots.tools",
	"ckan_filters": [{
		"key": "groups",
		"value": "dfw-publications"
	}],
	"ckan_datatype": {
		"@type": "Grassroots:Publication",
		"type_description": "Publication",
		"so:image": "https://grassroots.tools/grassroots/images/aiss/do
cumentmulti"
	}
}

~~~
