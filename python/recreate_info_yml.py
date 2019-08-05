#! /usr/bin/env python

import os
import sys
import yaml

models = []

if len(sys.argv) < 2:
    print("usage:\n\t recreate_info_yml [path_to_bagel_db] ")
else:
    db_folder = sys.argv[1]
    if not os.path.exists(db_folder):
        print("could not find db_folder: " + db_folder)
    else:
        indexMap = {}
        indexFile = os.path.join(db_folder, "info.yml")
        if os.path.isfile(indexFile):
            with open(indexFile) as f:
                indexMap = yaml.load(f)

        # scan db folder
        dirs = [f for f in os.listdir(db_folder)
                if os.path.isdir(os.path.join(db_folder, f))
                and f != "logs" and not f.startswith(".")]
        for d in sorted(dirs):
            versions = [v for v in sorted(os.listdir(os.path.join(db_folder, d)))
                        if os.path.isdir(os.path.join(db_folder, d, v))
                        and os.path.isfile(os.path.join(db_folder, d, v, "model.yml"))
                        ]
            if len(versions) >= 1:
                models.append({
                    "name": d,
                    # REVIEW this takes the type of the first version for all version
                    "type": yaml.load(
                                open(
                                    os.path.join(db_folder,
                                                 d,
                                                 versions[0],
                                                 "model.yml")
                                    )
                                )["type"],
                    "versions": [{"name": v} for v in versions]
                })

        yaml.dump({"models": models}, open(os.path.join(db_folder, "info.yml"), "w"))
