//
// Created by guoze.lin on 16/3/2.
//

#include "core_config_manager.h"
#include "common/atomic.h"
#include "constants.h"
#include "muduo/base/Logging.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAKE_CONFIG_PARSER(CLASS) (std::bind(&CLASS::parse,std::placeholders::_1,std::placeholders::_2))
#define MAKE_CONFIG_DELETER(CLASS) (std::bind(&CLASS::destruct,std::placeholders::_1))
#define MAKE_CONFIG(CONFIG_FILE,CLASS) ConfigValue(CONFIG_FILE,0,NULL,MAKE_CONFIG_PARSER(CLASS),NULL,MAKE_CONFIG_DELETER(CLASS))

namespace adservice{
    namespace server{

        using namespace muduo;
        using namespace adservice::utility::json;

        typedef typename ConfigManager::ConfigMap::iterator Iter;

        void* monitorConfigFiles(void* param){
            ConfigManager& configManager = ConfigManager::getInstance();
            while(configManager.isRuning()){
                sleep(5);
                configManager.load();
            }
        }

        void ConfigManager::load(){
            struct stat filestat;
            for(Iter iter = configMap.begin();iter!=configMap.end();iter++){
                ConfigManager::ConfigValue& configValue = iter->second;
                const std::string filePath = configValue.filePath;
                long version = configValue.version;
                void* data = configValue.data;
                if(stat(filePath.c_str(),&filestat)==-1){
                    LOG_ERROR<<"ConfigManager:can not stat file,"<<filePath<<",errno:"<<errno;
                    continue;
                }
                time_t lastModified = filestat.st_mtime;
                if(lastModified>version){ // 修改时间比当前版本更晚,更新
                    MessageWraper mw;
                    bool bSuccess = parseJsonFile(filePath.c_str(),mw);
                    if(!bSuccess){
                        LOG_ERROR<<"ConfigManager:parse config file error,"<<filePath;
                        continue;
                    }
                    ConfigObjectParser& parser = configValue.parser;
                    if(!parser){
                        LOG_ERROR<<"ConfigManager:parser of "<<iter->first<<" not assigned";
                        continue;
                    }
                    configValue.data = data = parser(mw,data);
                    configValue.version = lastModified;
                    ConfigChangeCallback& onChange = configValue.onChange;
                    if(onChange){
                        onChange(configValue.data);
                    }
                }
            }
        }

        void ConfigManager::start() {
            if(!ATOM_CAS(&started,0,1))
                return;
            // 注册配置解析器
            configMap[CONFIG_SERVICE]   =   MAKE_CONFIG(CONFIG_SERVICE_PATH,ServerConfig);
            configMap[CONFIG_CLICK]     =   MAKE_CONFIG(CONFIG_CLICK_PATH,ClickConfig);
            // 加载注册配置
            load();
            // 开启配置检测线程
            run = true;
            if(pthread_create(&monitorThread,NULL,&monitorConfigFiles,NULL)){
                LOG_ERROR<<"create remote config monitor error";
                return;
            }
            // 注册默认配置变更回调
            registerOnChange(CONFIG_CLICK,[](void*){ DebugMessage("click config change");});
        }

        void* ConfigManager::get(const std::string &configKey) {
            Iter iter;
            if((iter = configMap.find(configKey))==configMap.end()){
                throw ConfigException("ConfigManager no such config key");
            }
            ConfigValue& configValue = iter->second;
            if(configValue.data==NULL){
                const std::string& filePath = configValue.filePath;
                MessageWraper mw;
                bool bSuccess = parseJsonFile(filePath.c_str(),mw);
                if(!bSuccess){
                    throw ConfigException(std::string("ConfigManager:parse config file ")+filePath);
                }
                ConfigObjectParser& parser = configValue.parser;
                if(!parser){
                    throw ConfigException(std::string("ConfigManager:parser not assigned for key ")+iter->first);
                }
                configValue.data = parser(mw,configValue.data);
            }
            return configValue.data;
        }

        void ConfigManager::stop() {
            if(!ATOM_CAS(&started,1,0))
                return;
            if(run){
                run = false;
                configMap.clear();
                pthread_join(monitorThread,NULL);
            }
        }

        void ConfigManager::registerOnChange(const std::string& configKey,const ConfigChangeCallback& cb){
            Iter iter;
            if((iter = configMap.find(configKey))==configMap.end()){
                throw ConfigException("ConfigManager no such config key");
            }
            ConfigValue& configValue = iter->second;
            configValue.onChange = cb;
        }


        void ConfigManager::registerOnChange(const std::string& configKey,const ConfigChangeCallback&& cb){
            Iter iter;
            if((iter = configMap.find(configKey))==configMap.end()){
                throw ConfigException("ConfigManager no such config key");
            }
            ConfigValue& configValue = iter->second;
            configValue.onChange = cb;
        }

    }
}