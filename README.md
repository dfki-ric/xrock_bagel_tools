# xrock_bagel_tools

Bagel (Biologically inspired Graph-Based Language) is a cross-platform
graph-based dataflow language developed at the
[Robotics Innovation Center of the German Research Center for Artificial Intelligence (DFKI-RIC)](http://robotik.dfki-bremen.de/en/startpage.html)
and the [University of Bremen](http://www.informatik.uni-bremen.de/robotik/index_en.php).
It runs on (Ubuntu) Linux, Mac and Windows.

This package provides some command line tools to manage the XRock-Bagel
database. For more information type `bagel` in the terminal.


# Documentation {#documentation}

The main user documentation of Bagel can be found at:
https://github.com/dfki-ric/bagel_wiki/wiki

The following help is aslo shown when typing `bagel` in a terminal:

`add_to_bagel_db`:

Loads Bagel graphs and extern nodes into the Bagel model database.

	 usage: add_to_bagel_db [path_to_bagel_graphs] [path_to_bagel_db] [version] [git_address] [git_hash] force

path_to_bagel_graphs: path to the bagel_control folder <br />
path_to_bagel_db: path to the file database folder <br />
version: version string under wich the models are commited <br />
git_address: git address to checkout bagel_control folder <br />
git_hass: git hash pointing to commit used for the desired model version <br />
force: overwrite existing models, if version is already in database. Generally, this option should not be used, only if one pushes successfully a version and needs to bugfix that version.


`recreate_info_yml`:

Parses the given bagel_db and rewrites the info.yml from the result.

	 usage: recreate_info_yml [path_to_bagel_db]

`model_to_bagel`:

Converts and installs the Bagel models from the database into Bagel graphs and compiles extern nodes.
Note: Needs $AUTOPROJ_CURRENT_ROOT defined in environment.

	 usage: model_to_bagel --filedb=[optional path to file database]
	                       --modelname=[name of model to install]
	                       --version=[version to install]
	                       --skip_extern_nodes (optional / skip building of extern nodes)


`create_mars_config`:

Setups MARS configuration folder to load the Bagel graph into the simulation.
Note: Needs $AUTOPROJ_CURRENT_ROOT defined in environment.

	 usage: create_mars_config --filedb=[optional path to file database]
	                           --modelname=[name of model to install]
	                           --version=[version to install]
	                           --scene=[MARS scene file to load]
	                           --marsfolder=[optional / target folder for MARS configuration]


`init_parameters_to_graph`:

Transfers the init paramters from an existing start_paramters file each input port of the graph file
Note: Needs $AUTOPROJ_CURRENT_ROOT defined in environment.

	 usage: init_parameters_to_graph --init_file=[path to file holding the init values]
	                                               --graph_file=[path to graph file where the init values should be set]



`start_mars`:

Starts the MARS simulation for given model if the default configuration folder is installed.
Note: Needs $AUTOPROJ_CURRENT_ROOT defined in environment.

	 usage: start_mars --modelname=[name of model]
	                   --version=[version string]



`get_unreferenced_models`:

List all models that are not linked within another model.
Note: Either ${AUTOPROJ_CURRENT_ROOT}/bagel/bagel_db is used or the optional filedb argument.

	 usage: get_unreferenced_models --filedb=[path to file database]


`list_depending_models`:

List all models that include the model defined by the arguments.
Note: Either ${AUTOPROJ_CURRENT_ROOT}/bagel/bagel_db is used or the optional filedb argument.

	 usage: list_depending_models --filedb=[optional path to file database]
	                              --modelname=[name of model]
	                              --version=[version of model]
	                              --delete (optional / deletes model if no parent models found)


`bagel_model_info`:

List all models that include the model defined by the arguments.
Note: Either ${AUTOPROJ_CURRENT_ROOT}/bagel/bagel_db is used or the optional filedb argument.

	 usage: bagel_model_info --filedb=[optional path to file database]
	                         --modelname=[name of model]
	                         --version=[version of model]

## License

bagel_mars_deploy is distributed under the
[3-clause BSD license](https://opensource.org/licenses/BSD-3-Clause).
