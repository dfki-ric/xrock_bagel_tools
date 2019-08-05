#include <configmaps/ConfigMap.hpp>
#include <configmaps/ConfigAtom.hpp>
#include <mars/utils/misc.h>

using namespace configmaps;
using namespace mars::utils;
using namespace std;

bool checkModel(string modelName, string versionName, ConfigMap &models){

  for(auto it: models) {
    for(auto it2: (ConfigMap&)it.second) {
      ConfigMap &map = it2.second["versions"][0];
      if(map.hasKey("components")) {
        for(auto node: map["components"]["nodes"]) {
          if((string)node["model"]["name"] == modelName &&
             (string)node["model"]["version"] == versionName) {
            return true;
          }
        }
      }
    }
  }
  return false;
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

  string fileDB;
  if(config.hasKey("filedb")) {
    fileDB << config["filedb"];
  }
  else {
    char *envText = getenv("AUTOPROJ_CURRENT_ROOT");
    if(envText) {
      fileDB = envText;
      fileDB = pathJoin(fileDB, "bagel/bagel_db");
    }
    else {
      fprintf(stderr, "ERROR: no filedb nor AUTOPROJ_CURRENT_ROOT defined.\n");
      exit(-1);
    }
  }
  ConfigMap infoMap;
  try {
    std::string infoFile = pathJoin(fileDB, "info.yml");
    infoMap = ConfigMap::fromYamlFile(infoFile);
  } catch(...) {
    fprintf(stderr, "ERROR loading info.yml from db path\n");
    exit(-1);
  }

  ConfigMap models;
  fprintf(stderr, "load models: ");
  for(auto model: infoMap["models"]) {
    std::string modelName = model["name"];
    for(auto version: model["versions"]) {
      std::string versionName = version["name"];
      std::string modelPath = pathJoin(fileDB, modelName);
      modelPath = pathJoin(modelPath, versionName);
      modelPath = pathJoin(modelPath, "model.yml");
      try {
        models[modelName][versionName] = ConfigMap::fromYamlFile(modelPath);
      } catch(...) {
        fprintf(stderr, "ERROR: loading model: %s / %s\n", modelName.c_str(), versionName.c_str());
        exit(-1);
      }
      fprintf(stderr, ".");
    }
  }
  fprintf(stderr, "\n");

  ConfigMap resultMap;
  for(auto it: models) {
    std::string modelName = it.first;
    for(auto it2: (ConfigMap&)(it.second)) {
      std::string versionName = it2.first;
      if(!checkModel(modelName, versionName, models)) {
        resultMap[modelName].push_back(versionName);
      }
    }
  }

  fprintf(stderr, "\033[38;5;166mfound:\033[0m\n");
  for(auto it: resultMap) {
    fprintf(stderr, "  %s: %s", it.first.c_str(), it.second.toYamlString().c_str());
  }
  //fprintf(stderr, "loaded:\n%s\n", models.toYamlString().c_str());
  exit(0);
}
