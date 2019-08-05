#include <configmaps/ConfigMap.hpp>
#include <configmaps/ConfigAtom.hpp>
#include <configmaps/ConfigVector.hpp>
#include <mars/utils/misc.h>

using namespace configmaps;
using namespace mars::utils;
using namespace std;

bool checkModel(string modelName, string versionName, ConfigMap &model){

  ConfigMap &map = model["versions"][0];
  if(map.hasKey("components")) {
    for(auto node: map["components"]["nodes"]) {
      if((string)node["model"]["name"] == modelName &&
         (string)node["model"]["version"] == versionName) {
        return true;
      }
    }
  }
  return false;
}

ConfigMap getList(string modelName, string versionName, ConfigMap &models){
  ConfigMap resultMap;

  for(auto it: models) {
    for(auto it2: (ConfigMap&)it.second) {
      if(checkModel(modelName, versionName, (ConfigMap&)(it2.second))) {
        resultMap[it.first].push_back(it2.first);
      }
    }
  }
  return resultMap;
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
    fprintf(stderr, "No model name argument given: --modelname='model name'\n");
    exit(-1);
  }
  if(!config.hasKey("version")) {
    fprintf(stderr, "No model name argument given: --version='model verison'\n");
    exit(-1);
  }
  string mainModel = config["modelname"];
  string mainVersion = config["version"];
  string fileDB;
  bool deleteModel = false;
  if(config.hasKey("delete")) deleteModel = true;
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
      fprintf(stderr, "\033[31;1mERROR\033[0m: no filedb nor AUTOPROJ_CURRENT_ROOT defined.\n");
      exit(-1);
    }
  }
  ConfigMap infoMap;
  std::string infoFile = pathJoin(fileDB, "info.yml");
  try {
    infoMap = ConfigMap::fromYamlFile(infoFile);
  } catch(...) {
    fprintf(stderr, "\033[31;1mERROR\033[0m: loading info.yml from db path\n");
    exit(-1);
  }

  ConfigMap models;
  size_t modelIndex = 0, versionIndex = 0;
  bool foundModel = false, foundVersion = false;
  fprintf(stderr, "load models: ");
  size_t l=0;
  for(auto model: infoMap["models"]) {
    std::string modelName = model["name"];
    if(modelName == mainModel) {
      modelIndex = l;
      foundModel = true;
    }
    size_t k=0;
    for(auto version: model["versions"]) {
      std::string versionName = version["name"];
      if(foundModel and modelIndex == l and versionName == mainVersion) {
        versionIndex = k;
        foundVersion = true;
      }
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
      ++k;
    }
    ++l;
  }
  fprintf(stderr, "\n");

  if(!foundVersion) {
    fprintf(stderr, "ERROR: didn't found model in database\n");
    exit(-1);
  }
  ConfigMap resultMap = getList(mainModel, mainVersion, models);

  if(resultMap.size() == 0) {
    fprintf(stderr, "\nno parent models found\n");
    if(deleteModel) {
      fprintf(stderr, "delete model: %s version: %s\n\n", mainModel.c_str(), mainVersion.c_str());
      ConfigVector &v = infoMap["models"][modelIndex]["versions"];
      v.erase(v.begin()+versionIndex);
      std::string modelPath = pathJoin(fileDB, mainModel);
      modelPath = pathJoin(modelPath, mainVersion);
      system(("rm -rf " + modelPath).c_str());
      if(infoMap["models"][modelIndex]["versions"].size() == 0) {
        ConfigVector &v = infoMap["models"];
        v.erase(v.begin()+modelIndex);
        modelPath = pathJoin(fileDB, mainModel);
        system(("rm -rf " + modelPath).c_str());
      }
      infoMap.toYamlFile(infoFile);
    }
  }
  else {
    fprintf(stderr, "\n\033[38;5;166mfound depending models\033[0m:\n");
    for(auto it: resultMap) {
      fprintf(stderr, "  %s: %s", it.first.c_str(), it.second.toYamlString().c_str());
    }
    if(deleteModel) {
      fprintf(stderr, "\n\033[31;1mERROR\033[0m: can't delete model since others depend on it.\n\n");
      exit(-1);
    }
  }
  //fprintf(stderr, "loaded:\n%s\n", models.toYamlString().c_str());
  exit(0);
}
