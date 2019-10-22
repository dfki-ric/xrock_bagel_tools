#include <configmaps/ConfigMap.hpp>
#include <configmaps/ConfigAtom.hpp>
#include <configmaps/ConfigVector.hpp>
#include <mars/utils/misc.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <array>

using namespace configmaps;
using namespace mars::utils;
using namespace std;

string dbPath;
string rootDir;
ConfigMap modelsToLoad;
ConfigMap modelsLoaded;
ConfigMap nodeInfos;
string mainModel;
string mainVersion;
bool skipExternNodes = false;
ConfigMap externNodes;
vector<ConfigMap*> outGraphs;
ConfigMap deps;

void addExternNodeToList(ConfigMap &model, ConfigMap &version) {
  for(auto node: externNodes["nodes"]) {
    if(node["model"]["name"].getString() == (string)model["name"]) {
      if(node["version"]["name"].getString() != (string)version["name"]) {
        // handle error
        fprintf(stderr, "\033[31;1mERROR\033[0m: duplicated extern node: \n %s / %s\n %s / %s\n", node["model"]["name"].getString().c_str(), node["version"]["name"].getString().c_str(), model["name"].getString().c_str(), version["name"].getString().c_str());
        exit(-1);
      }
      return;
    }
  }
  externNodes["nodes"].push_back(ConfigMap());
  ConfigMap &m = ((ConfigVector&)externNodes["nodes"]).back();
  m["model"] = model;
  m["version"] = version;
}

ConfigMap loadModel(string name, string version) {
  std::string modelPath = pathJoin(dbPath, name);
  modelPath = pathJoin(modelPath, version);
  modelPath = pathJoin(modelPath, "model.yml");
  try {
    ConfigMap model = ConfigMap::fromYamlFile(modelPath);
    return model;
  } catch(...) {
    fprintf(stderr, "\033[31;1mERROR\033[0m: loading model %s / %s\n", name.c_str(), version.c_str());
    exit(-1);
  }
  return ConfigMap();
}

void fetchExternNode(ConfigMap &model, ConfigMap &version) {
  string modelName = model["name"];
  string modelVersion = version["name"];
  string cmd, cwd;
  fprintf(stderr, "  - fetch model: %s  version: %s\n", modelName.c_str(), modelVersion.c_str());

  // get install folder
  string folder = "tmp/bagel/extern_nodes/"+mainModel+"/"+mainVersion+"/"+modelName+"/"+modelVersion;
  folder = pathJoin(rootDir, folder);
  string installFolder = pathJoin(rootDir, "install/lib/bagel/"+mainModel+"/"+mainVersion);
  ConfigMap data = ConfigMap::fromYamlString(version["softwareData"]["data"]);
  if(!pathExists((folder))) {
    createDirectory(folder);
    cmd = "git clone " + (string)data["git"]["address"] + " " + folder;
    if(system(cmd.c_str()) != 0) {
      fprintf(stderr, "\033[31;1mERROR\033[0m: cloning extern node!\n");
      exit(-1);
    }
  }
  cwd = getCurrentWorkingDir();
  chdir(folder.c_str());
  // check if we have to checkout
  string hash = data["git"]["hash"];
  bool checkout = true;
  {
    array<char, 128> buffer;
    string result;
    shared_ptr<FILE> pipe(popen("git show --format='%H' --no-patch", "r"), pclose);
    if(!pipe) throw std::runtime_error("popen() failed!");
    while(!feof(pipe.get())) {
      if(fgets(buffer.data(), 128, pipe.get()) != nullptr)
        result += buffer.data();
    }
    if(result.find(hash) == 0) {
      checkout = false;
    }
  }
  if(checkout) {
    cmd = "git fetch --tags";
    if(system(cmd.c_str()) != 0) {
      fprintf(stderr, "\033[31;1mERROR\033[0m: fetch git!\n");
      exit(-1);
    }
    cmd = "git checkout " + hash;
    if(system(cmd.c_str()) != 0) {
      fprintf(stderr, "\033[31;1mERROR\033[0m: checkout hash id: %s\n", hash.c_str());
      exit(-1);
    }
  }
  folder = pathJoin(folder, data["git"]["subfolder"]);

  // get dependencies from manifest
  string manifestFile = pathJoin(folder, "manifest.xml");
  if(pathExists(manifestFile)) {
    ifstream infile(manifestFile);
    string line;
    while(getline(infile, line)) {
      vector<string> arrLine = explodeString('"', line);
      if(arrLine.size() < 3) continue;
      if(matchPattern("*depend package", arrLine[0])) {
        if(deps["local"].hasKey(arrLine[1])) continue;
        deps["local"][arrLine[1]] = true;
      }
      else if(matchPattern("*rosdep name", arrLine[0])) {
        if(deps["os"].hasKey(arrLine[1])) continue;
        deps["os"][arrLine[1]] = true;
      }
    }
  }
  chdir(cwd.c_str());
}

void buildDependencies() {
  // create a tmp manifest for deps
  string folder = "tmp/bagel/extern_nodes/"+mainModel+"/"+mainVersion+"/";
  folder = pathJoin(rootDir, folder);
  string manifest = pathJoin(folder, "manifest.xml");
  FILE *f = fopen(manifest.c_str(), "w");
  fprintf(f, "<package>\n");
  for(auto it: (ConfigMap&)(deps["local"])) {
    fprintf(f, "<depend package=\"%s\" />\n", it.first.c_str());
  }
  for(auto it: (ConfigMap&)(deps["os"])) {
    fprintf(f, "<rosdep name=\"%s\" />\n", it.first.c_str());
  }
  fprintf(f, "</package>\n");
  fclose(f);

  // create a tmp CMakeLists so that amake knows to build dependencies and nothing more
  string cmakelists = pathJoin(folder, "CMakeLists.txt");
  FILE *f2 = fopen(cmakelists.c_str(), "w");
  fprintf(f2, "cmake_minimum_required(VERSION 2.6)\n");
  fprintf(f2, "set(CMAKE_INSTALL_PREFIX .)\n");
  fprintf(f2, "INSTALL(FILES manifest.xml DESTINATION .)\n");
  fclose(f2);

  string cwd = getCurrentWorkingDir();
  chdir(folder.c_str());
  system("amake");
  chdir(cwd.c_str());
}

void buildExternNode(ConfigMap &model, ConfigMap &version) {
  string modelName = model["name"];
  string modelVersion = version["name"];
  string cmd, cwd;

  fprintf(stderr, "  - build model: %s  version: %s\n", modelName.c_str(), modelVersion.c_str());

  // get install folder
  string folder = "tmp/bagel/extern_nodes/"+mainModel+"/"+mainVersion+"/"+modelName+"/"+modelVersion;
  folder = pathJoin(rootDir, folder);
  string installFolder = pathJoin(rootDir, "install/lib/bagel/"+mainModel+"/"+mainVersion);
  ConfigMap data = ConfigMap::fromYamlString(version["softwareData"]["data"]);
  if(!pathExists((folder))) {
    fprintf(stderr, "\033[31;1mERROR\033[0m: path not found!\n");
    exit(-1);
  }
  cwd = getCurrentWorkingDir();
  chdir(pathJoin(folder, data["git"]["subfolder"]).c_str());
  bool reconfigure = true;
  if(!pathExists("build")) {
    createDirectory("build");
  }
  else {
    // check if we have to reconfigure; due to install prefix change
    if(pathExists("build/cmake_install.cmake")) {
      ifstream infile("build/cmake_install.cmake");
      string line;
      while(getline(infile, line)) {
        if(matchPattern("*set(CMAKE_INSTALL_PREFIX ", line)) {
          vector<string> arrLine = explodeString('"', line);
          if(arrLine.size() < 2) break;
          string prefix = mars::utils::trim(arrLine[1]);
          if(prefix.back() == '/') {
            prefix = prefix.substr(0, prefix.size()-1);
          }
          fprintf(stderr, "%s  -  %s\n", prefix.c_str(), installFolder.c_str());
          if(prefix == installFolder) {
            reconfigure = false;
          }
          break;
        }
      }
    }
  }
  chdir("build");
  if(reconfigure) {
    cmd = "cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX="+installFolder+" -DUseGitBranchInstall=OFF";
    if(system(cmd.c_str()) != 0) {
      fprintf(stderr, "\033[31;1mERROR\033[0m: configuring!\n");
      exit(-1);
    }
  }
  if(system("make install") != 0) {
    fprintf(stderr, "\033[31;1mERROR\033[0m: 'make install'!\n");
    exit(-1);
  }
  string outFolder = "install/share/bagel/models/"+mainModel+"/"+mainVersion+"/extern_nodes";
  outFolder = pathJoin(rootDir, outFolder);
  createDirectory(outFolder);
  cmd = "cp ../" + modelName + ".yml " + outFolder;
  system(cmd.c_str());
  chdir(cwd.c_str());
}

void convertModel(ConfigMap &model) {
  string modelName, modelVersion, key;
  modelName << model["name"];
  modelVersion << model["versions"][0]["name"];
  fprintf(stderr, "\033[38;5;166mINFO\033[0m: process model: %s version: %s\n", modelName.c_str(), modelVersion.c_str());
  ConfigMap *outGraph_ = new ConfigMap;
  ConfigMap &outGraph = *outGraph_;
  outGraph["graph"] = ConfigMap();
  ConfigMap &bagelGraph = outGraph["graph"];
  outGraph["initValues"] = ConfigMap();
  ConfigMap &initValues = outGraph["initValues"];
  bagelGraph["model"] = "bagel";
  ConfigMap &version = model["versions"][0];

  if(model["type"] == "bagel::extern") {
    addExternNodeToList(model, version);
    return;
  }
  ConfigMap layout;
  if(version.hasKey("components")) {
    ConfigMap data = ConfigMap::fromYamlString(version["softwareData"]["data"]);
    if(data.hasKey("gui") and data["gui"]["layouts"].hasKey("software")) {
      layout << data["gui"]["layouts"]["software"];
    }
    if(version["components"].hasKey("nodes")) {
      for(auto node: version["components"]["nodes"]) {
        modelName << node["model"]["name"];
        modelVersion << node["model"]["version"];
        key = modelName+"_"+modelVersion;
        ConfigMap info;
        if(nodeInfos.hasKey(modelName) and nodeInfos[modelName].hasKey(modelVersion)) {
          info = nodeInfos[modelName][modelVersion];
        }
        else {
          info = loadModel(modelName, modelVersion);
          nodeInfos[modelName][modelVersion] = info;
        }
        bagelGraph["nodes"].push_back(ConfigMap());
        ConfigMap &bagelInfo = ((ConfigVector&)bagelGraph["nodes"]).back();
        string name = node["name"];
        bagelInfo["name"] = name;
        if(layout.hasKey(name)) {
          bagelInfo["pos"]["x"] = layout[name]["x"];
          bagelInfo["pos"]["y"] = layout[name]["y"];
        }
        if(info["type"] == "bagel::atomic") {
          bagelInfo["type"] = info["name"];
          if((string)bagelInfo["type"] == "g0") {
            bagelInfo["type"] = ">0";
          }
        }
        else if(info["type"] == "bagel::subgraph") {
          string subfolder = "../../"+modelName+"/"+modelVersion;
          string subfile = subfolder + "/"+modelName+".yml";
          bagelInfo["type"] = "SUBGRAPH";
          bagelInfo["subgraph_name"] = subfile;
          bool haveModel = false;
          if(modelsLoaded.hasKey(modelName) and modelsLoaded[modelName].hasKey(modelVersion)) {
            haveModel = true;
          }
          else if(modelsToLoad.hasKey(modelName) and modelsToLoad[modelName].hasKey(modelVersion)) {
            haveModel = true;
          }
          if(!haveModel) {
            modelsToLoad[modelName][modelVersion] = info;
          }
        } else if(info["type"] == "bagel::extern") {
          bagelInfo["type"] = "EXTERN";
          bagelInfo["extern_name"] = modelName;
          bool haveModel = false;
          if(modelsLoaded.hasKey(modelName) and modelsLoaded[modelName].hasKey(modelVersion)) {
            haveModel = true;
          }
          else if(modelsToLoad.hasKey(modelName) and modelsToLoad[modelName].hasKey(modelVersion)) {
            haveModel = true;
          }
          if(!haveModel) {
            modelsToLoad[modelName][modelVersion] = info;
          }
        }
        int inCount = 0;
        int outCount = 0;
        for(auto interface_: info["versions"][0]["interfaces"]) {
          if(interface_["direction"] == "INCOMING") {
            bagelInfo["inputs"].push_back(ConfigMap());
            ConfigMap &port = ((ConfigVector&)bagelInfo["inputs"]).back();
            string portName = interface_["name"];
            port["name"] = portName;
            for(auto  conf: version["components"]["configuration"]["nodes"]) {
              if((string)conf["name"] == (string)node["name"]) {
                port["type"] = "SUM";
                port["bias"] = 0.0;
                port["default"] = 0.0;
                bool found_port_info = false;
                if(conf.hasKey("data")) {
                  ConfigMap config = ConfigMap::fromYamlString(conf["data"]);
                  if (config["interfaces"].hasKey(portName)) {
                    port["type"] = config["interfaces"][portName]["merge"];
                    port["bias"] = config["interfaces"][portName]["bias"];
                    port["default"] = config["interfaces"][portName]["default"];
                    found_port_info = true;
                  }
                }
                if (!found_port_info) {
                  fprintf(stderr, "WARNING: Didn't find interface information for port %s of node %s\n",
                          portName.c_str(), ((string)node["name"]).c_str());
                }
              }
            }
            ++inCount;
          }
          else if(interface_["direction"] == "OUTGOING") {
            bagelInfo["outputs"].push_back(ConfigMap());
            ConfigMap &port = ((ConfigVector&)bagelInfo["outputs"]).back();
            port["name"] = interface_["name"];
            ++outCount;
          }
        }
      }
    }
    if(version["components"].hasKey("edges")) {
      for(auto edge: version["components"]["edges"]) {
        bagelGraph["edges"].push_back(ConfigMap());
        ConfigMap &bagelEdge = ((ConfigVector&)bagelGraph["edges"]).back();
        bagelEdge["weight"] = 1.0;
        bagelEdge["smooth"] = true;
        bagelEdge["fromNode"] = edge["from"]["name"];
        bagelEdge["fromNodeOutput"] = edge["from"]["interface"];
        bagelEdge["toNode"] = edge["to"]["name"];
        bagelEdge["toNodeInput"] = edge["to"]["interface"];
        if(edge.hasKey("data")) {
          ConfigMap data = ConfigMap::fromYamlString(edge["data"]);
          if(data.hasKey("weight")) {
            bagelEdge["weight"] = data["weight"];
          }
          if(data.hasKey("ignore_for_sort")) {
            bagelEdge["ignore_for_sort"] = data["ignore_for_sort"];
          }
        }
      }
    }
  }
  for(auto interface_: version["interfaces"]) {
    // todo: interface names must be unique node names in the graph
    if(interface_["direction"] == "INCOMING") {
      bagelGraph["nodes"].push_back(ConfigMap());
      ConfigMap &port = ((ConfigVector&)bagelGraph["nodes"]).back();
      if(interface_.hasKey("data")) {
        ConfigMap data = ConfigMap::fromYamlString(interface_["data"]);
        if(data.hasKey("initValue")) {
          initValues[interface_["name"]] = data["initValue"];
        }
      }
      port["type"] = "INPUT";
      port["name"] = interface_["name"];
      string name = interface_["linkToNode"];
      if(layout.hasKey(name)) {
        port["pos"]["x"] = (int)(layout[name]["x"])-200;
        port["pos"]["y"] = layout[name]["y"];
      }
      bagelGraph["edges"].push_back(ConfigMap());
      ConfigMap &edge = ((ConfigVector&)bagelGraph["edges"]).back();
      edge["fromNode"] = interface_["name"];
      edge["fromNodeOutput"] = "out1";
      edge["toNode"] = name;
      edge["toNodeInput"] = interface_["linkToInterface"];
      // todo: store weigt in edge data
      edge["weight"] = 1.0;
      edge["smooth"] = true;
    }
    else if(interface_["direction"] == "OUTGOING") {
      bagelGraph["nodes"].push_back(ConfigMap());
      ConfigMap &port = ((ConfigVector&)bagelGraph["nodes"]).back();
      port["type"] = "OUTPUT";
      port["name"] = interface_["name"];
      port["inputs"][0]["name"] = "in1";
      port["inputs"][0]["type"] = "SUM";
      port["inputs"][0]["bias"] = "0.0";
      port["inputs"][0]["default"] = "0.0";
      string name = interface_["linkToNode"];
      if(layout.hasKey(name)) {
        port["pos"]["x"] = (int)(layout[name]["x"])-200;
        port["pos"]["y"] = layout[name]["y"];
      }
      bagelGraph["edges"].push_back(ConfigMap());
      ConfigMap &edge = ((ConfigVector&)bagelGraph["edges"]).back();
      edge["toNode"] = interface_["name"];
      edge["toNodeInput"] = "in1";
      edge["fromNode"] = name;
      edge["fromNodeOutput"] = interface_["linkToInterface"];
      edge["weight"] = 1.0;
      edge["smooth"] = true;
    }
  }
  bagelGraph["externNodePath"] = "../../extern_nodes";
  string outFolder = "install/share/bagel/models/"+mainModel+"/"+mainVersion+"/"+(string)model["name"]+"/"+(string)version["name"];
  outFolder = pathJoin(rootDir, outFolder);
  outGraph["folder"] = outFolder;
  outGraph["file"] = (string)model["name"]+".yml";
  outGraphs.push_back(outGraph_);
}


int main(int argc, char **argv) {
  deps["local"] = ConfigMap();
  deps["os"] = ConfigMap();
  ConfigMap config;
  for(int i=1; i<argc; ++i) {
    std::string arg = argv[i];
    if(arg.substr(0, 2) == "--") {
      arg = arg.substr(2);
    }
    std::vector<std::string> arrString = explodeString('=', arg);
    if(arrString.size() == 2) {
      config[arrString[0]] = arrString[1];
    }
    else {
      config[arrString[0]] = true;
    }
  }

  if(!config.hasKey("modelname")) {
    fprintf(stderr, "mising modelname argument: --modelname=[name of model]\n");
    exit(-1);
  }

  if(!config.hasKey("version")) {
    fprintf(stderr, "mising version argument: --version=[name of model]\n");
    exit(-1);
  }

  char *envText = getenv("AUTOPROJ_CURRENT_ROOT");
  if(envText) {
    rootDir = envText;
  }
  else {
    fprintf(stderr, "\033[31;1mERROR\033[0m: no AUTOPROJ_CURRENT_ROOT found in env variables\n");
    exit(-1);
  }

  if(config.hasKey("filedb")) {
    dbPath << config["filedb"];
    if(!pathExists((dbPath))) {
      // try to add root dir
      dbPath = pathJoin(rootDir, dbPath);
      if(!pathExists((dbPath))) {
        fprintf(stderr, "\033[31;1mERROR\033[0m: path given for filedb not found!\n");
        exit(-1);
      }
    }
  }
  else {
    dbPath = pathJoin(rootDir, "bagel/bagel_db");
  }
  mainModel << config["modelname"];
  mainVersion << config["version"];

  // compare installed folder hash with bagel_db
  // check if we have a hash file in target folder

  string installedHashFile = "install/share/bagel/models/"+mainModel+"/"+mainVersion+"/dbHash";
  installedHashFile = pathJoin(rootDir, installedHashFile);
  string hashFile = "tmp";///bagel_db_hash"
  createDirectory(hashFile);
  hashFile = pathJoin(rootDir, hashFile+"/dbHash");
  std::string cmd = "find " + dbPath + " -type f -print0 | xargs -0 shasum | shasum > " + hashFile;
  if(system(cmd.c_str()) != 0) {
    fprintf(stderr, "\033[31;1mERROR\033[0m: generate hash of bagel_db folder! continue build...\n");
  }
  else {
    if(pathExists((installedHashFile))) {
      std::ifstream t(hashFile.c_str()), t2(installedHashFile.c_str());
      std::stringstream dbHash, installedHash;
      dbHash << t.rdbuf();
      installedHash << t2.rdbuf();
      if(dbHash.str() == installedHash.str()) {
        fprintf(stderr, "\033[38;5;166mINFO\033[0m: No change found in bagel_db. Remove \"%s\" to rebuild model.\n", installedHashFile.c_str());
        exit(0);
      }
    }
  }
  modelsToLoad[mainModel][mainVersion] = loadModel(mainModel, mainVersion);
  if(config.hasKey("skip_extern_nodes")) {
    skipExternNodes = true;
  }
  while(modelsToLoad.size()) {
    ConfigMap copy = modelsToLoad;
    modelsToLoad.clear();
    for(auto it: copy) {
      for(auto it2: (ConfigMap&)(it.second)) {
        convertModel(it2.second);
        modelsLoaded[it.first][it2.first] = true;
      }
    }
  }
  if(!skipExternNodes) {
    fprintf(stderr, "\033[38;5;166mINFO\033[0m: fetch extern nodes:\n");
    for(auto node: externNodes["nodes"]) {
      fetchExternNode(node["model"], node["version"]);
    }
    fprintf(stderr, "\033[38;5;166mINFO\033[0m: build dependencies:\n");

    // handle deps
    if(deps["local"].size() or deps["os"].size()) {
      buildDependencies();
    }
    fprintf(stderr, "\033[38;5;166mINFO\033[0m: build extern nodes:\n");
    for(auto node: externNodes["nodes"]) {
      buildExternNode(node["model"], node["version"]);
    }
  }
  fprintf(stderr, "\033[38;5;166mINFO\033[0m: write bagel graphs\n");
  for(auto it: outGraphs) {
    createDirectory((*it)["folder"]);
    string outFile = pathJoin((*it)["folder"], (*it)["file"]);
    (*it)["graph"].toYamlFile(outFile);
    if((*it)["initValues"].size()) {
      outFile = pathJoin((*it)["folder"], "start_parameters.yml");
      (*it)["initValues"].toYamlFile(outFile);
    }
  }
  if(pathExists(hashFile)) {
    cmd = "cp " + hashFile + " " + installedHashFile;
    if(system(cmd.c_str()) != 0) {
      fprintf(stderr, "\033[31;1mERROR\033[0m: install hash of bagel_db!\n %s\n", cmd.c_str());
    }
  }
  exit(0);
}
