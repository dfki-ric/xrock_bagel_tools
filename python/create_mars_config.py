#! /usr/bin/env python

import os
import sys
import yaml

modelsLoaded = {}
modelsToLoad = {}
nodeInfos = {}
config = {}
workspacePath = "."

def makeDir(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        pass


for arg in sys.argv:
    arrArg = arg.split("=")
    if len(arrArg) == 2:
        config[arrArg[0][2:]] = arrArg[1]


devRootPath = ""#config["workspace"]
filedb = ""

if "workspace" in config:
    devRootPath = config["workspace"]
    if not os.path.isfile(os.path.join(devRootPath, "env.sh")):
        print("No env.sh found in workspace path; Try to load AUTOPROJ_CURRENT_ROOT.")
        if not 'AUTOPROJ_CURRENT_ROOT' in os.environ:
            print("No AUTOPROJ_CURRENT_ROOT found in environment.")
            exit(-1)
        else:
            devRootPath = os.environ['AUTOPROJ_CURRENT_ROOT']
    #the workspace path in not correctly given by the management gui
    workspacePath = "/".join(os.getcwd().split("/")[:-3])
    if not os.path.isfile(os.path.join(workspacePath, "generalsettings.yml")):
        print("cloud not find \"generalsettings.yml\" in " + workspacePath)
        exit(-1)
else:
    if not 'AUTOPROJ_CURRENT_ROOT' in os.environ:
        print("No AUTOPROJ_CURRENT_ROOT found in environment.")
        exit(-1)
    else:
        devRootPath = os.environ['AUTOPROJ_CURRENT_ROOT']

    if "filedb" in config:
        filedb = config["filedb"]
    else:
        filedb = os.path.join(devRootPath, "bagel/bagel_db")

# if not "workspace" in config:
#     print("No workspace argument given \"workspace='path to current dev root'\".")
#     exit(-1)

if not "modelname" in config:
    print("No modelname argument given \"--modelname='model name'\".")
    exit(-1)

if not "version" in config:
    print("No model version argument given \"--version='model version'\".")
    exit(-1)

modelName = config["modelname"]
modelVersion = config["version"]
modelDomain = "software"
if "domain" in config:
    modelDomain = config["domain"]

marsScene = ""
haveScene = False
if "scene" in config:
    haveScene = True
    marsScene = config["scene"]

# todo: store this link in the model data
bagelName = modelName
bagelVersion = modelVersion
if modelDomain == "assembly":
    bagelName = "bagel::"+modelName
    bagelVersion = modelName

# 1. copy mars_defautl to mars_config_folder
# 2. copy other_libs and core_libs
# 3. add BagelMARS to other_libs
# 4. configure BagelMARS
# 5. create smurf scene and add model
# 6. create start_script to start mars with smurf scene

# 1:
if "marsfolder" in config:
    targetFolder = config["marsfolder"]
else:
    targetFolder = os.path.join(devRootPath, "install/configuration/mars/"+modelName+"/"+modelVersion)

if not os.path.exists(targetFolder):
    config["replace"] = True
    makeDir(targetFolder)

marsDefaultFolder = os.path.join(devRootPath, "install/configuration/mars_default")
if not os.path.exists(marsDefaultFolder):
    print("Could not find MARS default configuration.")
    exit(-1)

if "replace" in config:
    cmd = "cp -r " + marsDefaultFolder + "/* " + targetFolder
    os.system(cmd)

# 2:
if "replace" in config:
    cmd = "cp " + os.path.join(targetFolder, "core_libs.txt.example") + " " + os.path.join(targetFolder, "core_libs.txt")
    os.system(cmd)
    cmd = "cp " + os.path.join(targetFolder, "other_libs.txt.example") + " " + os.path.join(targetFolder, "other_libs.txt")
    os.system(cmd)

    # 3:
    cmd = "echo \"BagelMARS\\n\" >> " + os.path.join(targetFolder, "other_libs.txt")
    os.system(cmd)

# 4:
bagelFile = os.path.join(devRootPath, "install/share/bagel/models/"+bagelName+"/"+bagelVersion+"/"+bagelName+"/"+bagelVersion+"/"+bagelName+".yml")
initFile = os.path.join(devRootPath, "install/share/bagel/models/"+bagelName+"/"+bagelVersion+"/"+bagelName+"/"+bagelVersion+"/start_parameters.yml")
externPath = os.path.join(devRootPath, "install/lib/bagel/"+bagelName+"/"+bagelVersion+"/lib/bagel/")
bagelMARSConfig = {"externNodesPath": externPath, "GraphFilename": bagelFile}
if os.path.exists(initFile):
    bagelMARSConfig["StartParameters"] = initFile
bagelConfigFile = os.path.join(targetFolder, "BagelMARS.yml")
with open(bagelConfigFile, "w") as f:
    yaml.dump(bagelMARSConfig, f)

# 5:
# add use of alternative model
sceneFile = ""
if haveScene:
    if len(marsScene) > 0:
        smurfScene = {}
        smurfScene["entities"] = []
        arrFiles = marsScene.split(";")
        i = 1
        for file in arrFiles:
            if file[0] == "/":
                sceneFile = file
            else:
                sceneFile = os.path.relpath(file, targetFolder)
            entity = {}
            entity["file"] = sceneFile
            entity["name"] = "model"+str(i)
            smurfScene["entities"].append(entity)
            i += 1
        sceneFile = "Scene.smurfs"
        smurfsFile = os.path.join(targetFolder, sceneFile)
        with open(smurfsFile, "w") as f:
            yaml.dump(smurfScene, f)
else :
    sceneFile = "Scene.smurfs"
    smurfScene = {}
    smurfScene["entities"] = []
    entity = {}
    smurfFile = os.path.join(workspacePath, "tmp/models/"+modelDomain+"/"+modelName+"/"+modelVersion+"/smurf/"+modelName+".smurf")
    entity["file"] = smurfFile
    entity["name"] = modelName
    smurfScene["entities"].append(entity)
    smurfsFile = os.path.join(targetFolder, sceneFile)
    with open(smurfsFile, "w") as f:
        yaml.dump(smurfScene, f)

# 6. create start_script to start mars with smurf scene
marsStartFile = os.path.join(targetFolder, "start_mars.sh")
with open(marsStartFile, "w") as f:
    f.write("#! /usr/bin/env bash\n\n")
    if len(sceneFile) > 0:
        f.write("DYLD_LIBRARY_PATH=$MCDYLD_LIBRARY_PATH mars_app -s \""+sceneFile+"\"\n")
    else:
        f.write("DYLD_LIBRARY_PATH=$MCDYLD_LIBRARY_PATH mars_app\n")

cmd = "chmod +x " + marsStartFile
os.system(cmd)


status = {"status": 0, "message": "closed fine"}
with open("status.yml", "w") as f:
    yaml.dump(status, f)
