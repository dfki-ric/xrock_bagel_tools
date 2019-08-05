#! /usr/bin/env python

import os
import yaml
import argparse

#--------------------     MAIN    -----------------
if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Inserts init values in a magic graph from a given start parameters file')
    parser.add_argument('--init_file', help='path to init file', default=".")
    parser.add_argument('--graph_file', help='path to magic graph file', default=".")
    args = parser.parse_args()

    # read start parameters file
    init_dict = []
    with open(args.init_file, 'r') as f1:
        init_dict = yaml.load(f1)

    # read magic graph file
    graph_dict = {}
    with open(args.graph_file, 'r') as f2:
        graph_dict = yaml.load(f2)
        interfaces = graph_dict["versions"][0]["interfaces"]

        # overwrite init values
        for interface in interfaces:
            if interface["direction"] == "INCOMING":
                if interface["name"] in init_dict:
                    data_map = {}
                    if "data" in interface:
                        data_map = yaml.load(interface["data"])
                    data_map["initValue"] = init_dict[interface["name"]]
                    interface["data"] = yaml.dump(data_map)

    # write file
    with open(args.graph_file, 'w') as f3:
        yaml.dump(graph_dict, f3, default_flow_style=False)
