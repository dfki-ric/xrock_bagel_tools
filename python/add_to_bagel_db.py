#! /usr/bin/env python

import os
import sys
import yaml

models = {}
newVersion = ""
gitAddress = ""
gitHash = ""
forcePush = False
bagel_folder = ""
atomicList = ["SIN", "PIPE", "COS", "ASIN", "TAN", "ABS", "DIVIDE", "SQRT"]
twoInTypes = ["POW", "MOD"]
threeInTypes = [">0", "==0"]

def makeDir(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        pass

def getExternInterfaceNames(externName, inList, outList):
    global bagel_folder
    externFile = externName + ".yml"
    map = {}
    found = False
    for root, dirs, files in os.walk(bagel_folder):
        for name in files:
            if name == externFile:
                found = True
                with open(os.path.join(root, name)) as f:
                    map = yaml.load(f)
                break
        if found:
            break
    if "inputs" in map:
        for port in map["inputs"]:
            inList.append(port["name"])
    if "outputs" in map:
        for port in map["outputs"]:
            outList.append(port["name"])

def getSubgraphInterfaceNames(graphName, inList, outList):
    global bagel_folder
    graphFile = graphName + ".yml"
    map = {}
    found = False
    for root, dirs, files in os.walk(bagel_folder):
        for name in files:
            if name == graphFile:
                found = True
                with open(os.path.join(root, name)) as f:
                    map = yaml.load(f)
                break
        if found:
            break
    if "nodes" in map:
        for node in map["nodes"]:
            if node["type"] == "INPUT":
                inList.append(node["name"])
            elif node["type"] == "OUTPUT":
                outList.append(node["name"])


def loadExternModel(modelMap, gitInfo):
    global models, newVersion
    if modelMap["name"] in models:
        print("ERROR (loadExternModel): duplicated model info: " + modelMap["name"])
        return False
    model = {}
    model["name"] = modelMap["name"]
    model["type"] = "bagel::extern"
    model["domain"] = "SOFTWARE"
    model["versions"] = []
    version = {}
    version["interfaces"] = []
    config = {"interfaces": {}}
    inMap = {"direction": "INCOMING", "type": "double", "name": ""}
    defConfig = {"bias": 0.0, "default": 0.0, "merge": "SUM"}
    for interface in modelMap["inputs"]:
        inMap["name"] = interface["name"]
        version["interfaces"].append(inMap.copy())
        config["interfaces"][inMap["name"]] = defConfig.copy()
    inMap["direction"] = "OUTGOING"
    for interface in modelMap["outputs"]:
        inMap["name"] = interface["name"]
        version["interfaces"].append(inMap.copy())
    version["maturity"] = "INPROGRESS"
    version["defaultConfig"] = {"data": yaml.dump(config).strip()}
    version["name"] = newVersion
    version["softwareData"] = {"data": yaml.dump(gitInfo).strip()}
    model["versions"].append(version)
    #print(yaml.dump(model))
    models[modelMap["name"]] = model
    return True

def getConnections(node, edges):
    edgeList = []
    for edge in edges:
        if not "fromNode" in edge:
            # will fail and be handled later
            continue
        if edge["fromNode"] == node or edge["toNode"] == node:
            edgeList.append(edge.copy())
    return edgeList

def getOutConnections(node, interface, edges):
    edgeList = []
    for edge in edges:
        if not "fromNode" in edge:
            # will fail and be handled later
            continue
        if edge["fromNode"] == node and edge["fromNodeOutput"] == interface:
            edgeList.append(edge.copy())
    return edgeList

def getInConnections(node, interface, edges):
    edgeList = []
    for edge in edges:
        if not "toNode" in edge:
            # will fail and be handled later
            continue
        if edge["toNode"] == node and edge["toNodeInput"] == interface:
            edgeList.append(edge.copy())
    return edgeList

def validateEdge(edge, nodeInfoMap):
    global models

    if not edge["to"]["name"] in nodeInfoMap:
        print("ERROR could not validate edge because node info is missing for: "+edge["to"]["name"])
        return False

    nodeInfo = nodeInfoMap[edge["to"]["name"]]
    found = False
    for i in nodeInfo["inList"]:
        if i == edge["to"]["interface"]:
            found = True
    if not found:
        print("ERROR could not validate input interface")
        return False

    if not edge["from"]["name"] in nodeInfoMap:
        print("ERROR could not validate edge because node info is missing for: "+edge["from"]["name"])
        return False

    nodeInfo = nodeInfoMap[edge["from"]["name"]]
    found = False
    for i in nodeInfo["outList"]:
        if i == edge["from"]["interface"]:
            found = True
    if not found:
        print("ERROR could not validate input interface")
        return False

    return True

def validateInterface(interface, nodeInfoMap):
    global models

    if not interface["linkToNode"] in nodeInfoMap:
        print("ERROR could not validate interface because node info is missing for: "+interface["linkToNode"])
        return False

    nodeInfo = nodeInfoMap[interface["linkToNode"]]
    found = False
    iList = []
    if interface["direction"] == "INCOMING":
        iList = nodeInfo["inList"]
    else:
        iList = nodeInfo["outList"]

    for i in iList:
        if i == interface["linkToInterface"]:
            return True

    print("ERROR could not validate interace interface")
    return False

def getNodeType(nodeName, nodeList):
    for node in nodeList:
        if node["name"] == nodeName:
            return node["type"]
    return ""

def loadGraphModel(modelName, modelMap):
    global models, newVersion, twoInTypes, threeInTypes
    if modelName in models:
        print("ERROR (loadGraphModel): duplicated model info: " + name)
        return False
    model = {}
    model["name"] = modelName
    model["type"] = "bagel::subgraph"
    model["domain"] = "SOFTWARE"
    model["versions"] = []
    gui = {"defaultLayout": "software", "layouts": {"software": {}}}
    version = {}
    version["interfaces"] = []
    config = {"interfaces": {}}
    inMap = {"direction": "INCOMING", "type": "double", "name": ""}
    defConfig = {"bias": 0.0, "default": 0.0, "merge": "SUM"}
    version["components"] = {"nodes": [], "edges": []}
    version["components"]["configuration"] = {"nodes": []}
    nodeInfoMap = {}
    nodeNameMap = {}
    # dict of nodes / interfaces old -> new
    atomicInterfaceRemapping = {}
    edgeFilterList = []
    for node in modelMap["nodes"]:
        newNode = {}
        # if no name in node use id
        if not "name" in node:
            print("ERROR: Old bagel version! Maybe load and save again with new BagelGui.")
            return False
        else:
            newNode["name"] = node["name"]
        newNode["model"] = {}
        newNode["model"]["domain"] = "SOFTWARE"
        newNode["model"]["version"] = newVersion
        # this lists are needed to map the old idx based edges
        # to the name based
        inList = []
        outList = []

        if node["type"] == "EXTERN":
            newNode["model"]["name"] = node["extern_name"]
            getExternInterfaceNames(node["extern_name"], inList, outList)
        elif node["type"] == "SUBGRAPH":
            subgraphName = node["subgraph_name"].split("/")[-1][:-4]
            newNode["model"]["name"] = subgraphName
            getSubgraphInterfaceNames(subgraphName, inList, outList)
        elif node["type"] == "INPUT":
            # check how much edges are connected
            edgeList = []
            if "edges" in modelMap:
                edgeList = getConnections(node["name"], modelMap["edges"])
            if len(edgeList) == 1 and edgeList[0]["weight"] == 1.0:
                if not getNodeType(edgeList[0]["toNode"], modelMap["nodes"]) == "OUTPUT":
                    parentEdgeList = getInConnections(edgeList[0]["toNode"], edgeList[0]["toNodeInput"], modelMap["edges"])
                    if len(parentEdgeList) == 1:
                        # export parent interface
                        inMap["direction"] = "INCOMING"
                        inMap["name"] = node["name"]
                        inMap["linkToInterface"] = edgeList[0]["toNodeInput"]
                        inMap["linkToNode"] = edgeList[0]["toNode"]
                        version["interfaces"].append(inMap.copy())
                        # add interface config
                        config["interfaces"][node["name"]] = defConfig.copy()
                        edgeFilterList.append(node["name"])
                        continue

            inList.append("in1")
            outList.append("out1")
            newNode["model"]["name"] = "PIPE"
            newNode["model"]["version"] = "1.0.0"
            # namespace node name to have no conflict with interface name
            newNode["name"] = "INPUT::"+node["name"]
            # add interface config
            config["interfaces"][node["name"]] = defConfig.copy()
            # add interface
            inMap["direction"] = "INCOMING"
            inMap["name"] = node["name"]
            inMap["linkToInterface"] = "in1"
            inMap["linkToNode"] = newNode["name"]
            version["interfaces"].append(inMap.copy())
            # store name mapping for edges
            nodeNameMap[node["name"]] = newNode["name"]
        elif node["type"] == "OUTPUT":
            # check how much edges are connected
            edgeList = []
            if "edges" in modelMap:
                edgeList = getConnections(node["name"], modelMap["edges"])
            if len(edgeList) == 1 and edgeList[0]["weight"] == 1.0:
                if not getNodeType(edgeList[0]["fromNode"], modelMap["nodes"]) == "INPUT":
                    parentEdgeList = getOutConnections(edgeList[0]["fromNode"], edgeList[0]["fromNodeOutput"], modelMap["edges"])
                    if len(parentEdgeList) == 1:
                        # export parent interface
                        inMap["direction"] = "OUTGOING"
                        inMap["name"] = node["name"]
                        inMap["linkToInterface"] = edgeList[0]["fromNodeOutput"]
                        inMap["linkToNode"] = edgeList[0]["fromNode"]
                        version["interfaces"].append(inMap.copy())
                        edgeFilterList.append(node["name"])
                        continue

            inList.append("in1")
            outList.append("out1")
            newNode["model"]["name"] = "PIPE"
            newNode["model"]["version"] = "1.0.0"
            # namespace node name to have no conflict with interface name
            newNode["name"] = "OUTPUT::"+node["name"]
            # add interface
            inMap["direction"] = "OUTGOING"
            inMap["name"] = node["name"]
            inMap["linkToInterface"] = "out1"
            inMap["linkToNode"] = newNode["name"]
            version["interfaces"].append(inMap.copy())
            # store name mapping for edges
            nodeNameMap[node["name"]] = newNode["name"]
        else:
            inList.append("in1")
            outList.append("out1")
            if node["type"] in twoInTypes:
                inList.append("in2")
            if node["type"] in threeInTypes:
                inList.append("in2")
                inList.append("in3")

            inputList = []
            if type(node["inputs"]) is dict:
                inputList.append(node["inputs"])
            else:
                inputList = node["inputs"]
            if len(inputList) != len(inList):
                print("ERROR processing node input names: " + node["name"])
                return False

            outputList = []
            if type(node["outputs"]) is dict:
                outputList.append(node["outputs"])
            else:
                outputList = node["outputs"]
            if len(outputList) != len(outList):
                print("ERROR processing node output names: " + node["name"])
                return False

            # handle interface name remapping
            i = 0
            remap = {"in": {}, "out": {}}
            for interface in inputList:
                if interface["name"] != inList[i]:
                    remap["in"][interface["name"]] = inList[i]
                i += 1
            i = 0
            for interface in outputList:
                if interface["name"] != outList[i]:
                    remap["out"][interface["name"]] = outList[i]
                i += 1
            if len(remap["in"]) > 0 or len(remap["out"]) > 0:
                   atomicInterfaceRemapping[node["name"]] = remap

            newNode["model"]["name"] = node["type"]
            newNode["model"]["version"] = "1.0.0"

        # todo: add input config to configuration
        interfaceConf = {"interfaces": {}}
        if "inputs" in node:
            i = 0
            inputList = []
            if type(node["inputs"]) is dict:
                inputList.append(node["inputs"])
            else:
                inputList = node["inputs"]
            for it in inputList:
                name = inList[i]
                # todo: load input name from model
                interfaceConf["interfaces"][name] = {"merge": it["type"], "bias": it["bias"], "default": it["default"]}
                i += 1
            version["components"]["configuration"]["nodes"].append({"name": newNode["name"], "data": yaml.dump(interfaceConf).strip()})
        if "pos" in node:
            gui["layouts"]["software"][newNode["name"]] = node["pos"]
        version["components"]["nodes"].append(newNode)
        nodeInfoMap[newNode["name"]] = {"inList": inList, "outList": outList}

    for interface in version["interfaces"]:
        if interface["linkToNode"] in nodeNameMap:
            if interface["direction"] == "INCOMING":
                interface["linkToInterface"] = "in1"
            else:
                interface["linkToInterface"] = "out1"
        elif interface["linkToNode"] in atomicInterfaceRemapping:
            if interface["direction"] == "INCOMING":
                remap = atomicInterfaceRemapping[interface["linkToNode"]]["in"]
                if interface["linkToInterface"] in remap:
                    interface["linkToInterface"] = remap[interface["linkToInterface"]]
            else:
                remap = atomicInterfaceRemapping[interface["linkToNode"]]["out"]
                if interface["linkToInterface"] in remap:
                    interface["linkToInterface"] = remap[interface["linkToInterface"]]
        if not validateInterface(interface, nodeInfoMap):
            print("ERROR: Old bagel version! Maybe load and save again with new BagelGui.")
            return False

    edgeI = 1
    for edge in modelMap["edges"]:
        if not "fromNode" in edge:
            print("ERROR: Old bagel version! Maybe load and save again with new BagelGui.")
            return False
        if edge["fromNode"] in edgeFilterList or edge["toNode"] in edgeFilterList:
            continue
        newEdge = {}
        newEdge["name"] = str(edgeI)
        newEdge["from"] = {}
        name = edge["fromNode"]
        newEdge["from"]["interface"] = edge["fromNodeOutput"]
        if name in nodeNameMap:
            name = nodeNameMap[name]
            newEdge["from"]["interface"] = "out1"
        elif name in atomicInterfaceRemapping:
            remap = atomicInterfaceRemapping[name]["out"]
            if newEdge["from"]["interface"] in remap:
                newEdge["from"]["interface"] = remap[newEdge["from"]["interface"]]
        newEdge["from"]["domain"] = "SOFTWARE"
        newEdge["from"]["name"] = name
        name = edge["toNode"]
        newEdge["to"] = {}
        newEdge["to"]["interface"] = edge["toNodeInput"]
        if name in nodeNameMap:
            name = nodeNameMap[name]
            newEdge["to"]["interface"] = "in1"
        elif name in atomicInterfaceRemapping:
            remap = atomicInterfaceRemapping[name]["in"]
            if newEdge["to"]["interface"] in remap:
                newEdge["to"]["interface"] = remap[newEdge["to"]["interface"]]
        newEdge["to"]["domain"] = "SOFTWARE"
        newEdge["to"]["name"] = name
        if not validateEdge(newEdge, nodeInfoMap):
            print("ERROR: Old bagel version! Maybe load and save again with new BagelGui.")
            return False
        data = {"weight": edge["weight"], "smooth": True, "decouple": False, "ignore_for_sort": 0}
        if "ignore_for_sort" in edge:
            data["ignore_for_sort"] = edge["ignore_for_sort"]
        if "decouple" in edge:
            data["decouple"] = edge["decouple"]
        newEdge["data"] = yaml.dump(data).strip()
        version["components"]["edges"].append(newEdge)
        edgeI += 1

    dataMap = {}
    dataMap["gui"] = gui
    dataMap["description"] = {"markdown": "add description"}
    if "descriptions" in modelMap:
        dataMap["description"]["nodes"] = []
        dList = []
        if type(modelMap["descriptions"]) is dict:
            dList.append(modelMap["descriptions"])
        else:
            dList = modelMap["descriptions"]
        for d in dList:
            dataMap["description"]["nodes"].append(d.copy())
    version["maturity"] = "INPROGRESS"
    version["defaultConfig"] = {"data": yaml.dump(config).strip()}
    version["softwareData"] = {"data": yaml.dump(dataMap).strip()}
    version["name"] = newVersion
    model["versions"].append(version)
    models[modelName] = model
    return True

def loadModelsFromFolder(folder):
    global bagel_folder, gitAddress, gitHash
    for entry in os.listdir(folder):
        fullPath = os.path.join(folder, entry)
        if entry.endswith(".yml"):
            map = {}
            print("check yaml file: " + fullPath)
            with open(fullPath) as f:
                map = yaml.load(f)
            if "nodes" in map:
                print("found graph: " + entry[:-4] + " at: " + folder)
                if not loadGraphModel(entry[:-4], map):
                    return False
            elif "inputs" in map and map["name"] != "__RENAME__":
                print("found extern node: " + map["name"] + " at: " + folder)
                subFolder = os.path.relpath(folder, bagel_folder)
                gitInfo = {"git": {"address": gitAddress, "hash": gitHash, "subfolder": subFolder}}
                if not loadExternModel(map, gitInfo):
                    return False
        elif os.path.isdir(fullPath):
            if not loadModelsFromFolder(fullPath):
                return False
    return True

def writeModel(model, dbFolder, indexMap):
    found = False
    modelName = model["name"]
    modelVersion = model["versions"][0]["name"]
    for modelInfo in indexMap["models"]:
        if modelInfo["name"] == modelName:
            found = True
            foundVersion = False
            for v in modelInfo["versions"]:
                if v["name"] == modelVersion:
                    foundVersion = True
                    break
            if not foundVersion:
                modelInfo["versions"].append({"name": modelVersion})
            break
    if not found:
        modelInfo = {"name": modelName, "type": model["type"], "versions": []}
        modelInfo["versions"].append({"name": modelVersion})
        indexMap["models"].append(modelInfo)
    outFolder = os.path.join(dbFolder, modelName + "/" + modelVersion)
    makeDir(outFolder)
    outFile = os.path.join(outFolder, "model.yml")
    with open(outFile, "w") as f:
        yaml.dump(model, f)

if len(sys.argv) < 6:
    print("usage:\n\t add_to_bagel_db [path_to_bagel_control] [path_to_bagel_db] [version] [giturl] [hashid] force")

bagel_folder = sys.argv[1]
if not os.path.exists(bagel_folder):
    print("could not find bagel_folder: " + bagel_folder)

db_folder = sys.argv[2]
if not os.path.exists(db_folder):
    print("could not find db_folder: " + db_folder)

newVersion = sys.argv[3]
gitAddress = sys.argv[4]
gitHash = sys.argv[5]

if len(sys.argv) > 6:
    if sys.argv[6] == "force":
        forcePush = True

indexMap = {}
indexFile = os.path.join(db_folder, "info.yml")
if os.path.isfile(indexFile):
    with open(indexFile) as f:
        indexMap = yaml.load(f)

# check for atomic types:
if not "models" in indexMap:
    indexMap["models"] = []

for a in atomicList + twoInTypes + threeInTypes:
    found = False
    for model in indexMap["models"]:
        if model["name"] == a:
            for v in model["versions"]:
                if v["name"] == "1.0.0":
                    found = True
                    break
            break
    if not found:
        aModel = {}
        aModel["name"] = a
        aModel["type"] = "bagel::atomic"
        aModel["domain"] = "SOFTWARE"
        aModel["maturity"] = "INPROGRESS"
        aModel["versions"] = []
        version = {}
        config = {"interfaces": {}}
        version["interfaces"] = []
        version["name"] = "1.0.0"
        defConfig = {"bias": 0.0, "default": 0.0, "merge": "SUM"}
        inMap = {"direction": "INCOMING", "type": "double", "name": "in1"}
        version["interfaces"].append(inMap.copy())
        config["interfaces"]["in1"] = defConfig.copy()
        if a in twoInTypes or a in threeInTypes:
            inMap["name"] = "in2"
            version["interfaces"].append(inMap.copy())
            config["interfaces"]["in2"] = defConfig.copy()
        if a in threeInTypes:
            inMap["name"] = "in3"
            version["interfaces"].append(inMap.copy())
            config["interfaces"]["in3"] = defConfig.copy()
        inMap["direction"] = "OUTGOING"
        inMap["name"] = "out1"
        version["interfaces"].append(inMap.copy())
        version["defaultConfiguration"] = {"data": yaml.dump(config).strip()}
        aModel["versions"].append(version)
        models[a] = aModel

if not loadModelsFromFolder(bagel_folder):
    print("Abort with error!")

# now we have all models collected
# check if models with desired version are not already in database

writeModels = True
for key,newModel in models.items():
    found = False
    for model in indexMap["models"]:
        if model["name"] == newModel["name"]:
            if model["type"] != newModel["type"]:
                print("ERROR: "+newModel["name"]+" with wrong type in db: " + model["type"] + " instead of desired type: " + newModel["type"])
                writeModels = False
                break
            for v in model["versions"]:
                if v["name"] == newVersion:
                    found = True
                    break
            break
    if found:
        if forcePush:
            print("WARNING: overwrite " + newModel["name"] + " with version: " + newVersion)
        else:
            writeModels = False
            print("ERROR: " + newModel["name"] + " found already in db with version: " + newVersion)

if writeModels:
    for key,newModel in models.items():
        writeModel(newModel, db_folder, indexMap)
    with open(indexFile, "w") as f:
        yaml.dump(indexMap, f)
