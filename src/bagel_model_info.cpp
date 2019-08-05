#include <configmaps/ConfigMap.hpp>
#include <configmaps/ConfigAtom.hpp>
#include <configmaps/ConfigVector.hpp>
#include <mars/utils/misc.h>
#include <fstream>
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


void parseModel(ConfigMap &model, int depth) {
  string modelName, modelVersion;
  modelName << model["name"];
  modelVersion << model["versions"][0]["name"];
  ConfigMap &version = model["versions"][0];

  if(model["type"] == "bagel::extern") {
    for(int i=0; i<depth; ++i) {
      fprintf(stderr, "  ");
    }
    fprintf(stderr, "%d %s (%s) (extern)\n", depth, modelName.c_str(), modelVersion.c_str());
    return;
  }
  if(model["type"] == "bagel::subgraph") {
    for(int i=0; i<depth; ++i) {
      fprintf(stderr, "  ");
    }
    fprintf(stderr, "%d %s (%s) (submodel)\n", depth, modelName.c_str(), modelVersion.c_str());
  }
  if(version.hasKey("components")) {
    if(version["components"].hasKey("nodes")) {
      for(auto node: version["components"]["nodes"]) {
        modelName << node["model"]["name"];
        modelVersion << node["model"]["version"];
        ConfigMap info;
        if(nodeInfos.hasKey(modelName) and nodeInfos[modelName].hasKey(modelVersion)) {
          info = nodeInfos[modelName][modelVersion];
        }
        else {
          info = loadModel(modelName, modelVersion);
          nodeInfos[modelName][modelVersion] = info;
        }
        if(info["type"] == "bagel::subgraph") {
          parseModel(info, depth+1);
        } else if(info["type"] == "bagel::extern") {
          parseModel(info, depth+1);
        }
      }
    }
  }
}


int main(int argc, char **argv) {
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
  }
  else {
    dbPath = pathJoin(rootDir, "bagel/bagel_db");
  }
  mainModel << config["modelname"];
  mainVersion << config["version"];
  ConfigMap info = loadModel(mainModel, mainVersion);
  parseModel(info, 0);
  exit(0);
}
