//
// Created by guoze.lin on 16/4/29.
//

#ifndef ADCORE_ABSTRACT_QUERY_TASK_H
#define ADCORE_ABSTRACT_QUERY_TASK_H

#include "core_service.h"
#include "utility/utility.h"
#include "protocol/log/log.h"
#include "common/types.h"
#include "task_thread_data.h"
#include "logpusher/log_pusher.h"
#ifdef USE_ENCODING_GZIP
#include "muduo/net/ZlibStream.h"
#endif
#include <exception>

namespace adservice {

    namespace corelogic {

        using namespace muduo;
        using namespace muduo::net;
        using namespace adservice::utility::serialize;
        using namespace adservice::utility::cypher;
        using namespace adservice::utility::url;
        using namespace adservice::utility::hash;
        using namespace adservice::utility::file;


        /**
         * 处理请求的抽象逻辑
         */
        class AbstractQueryTask{
        public:
            explicit AbstractQueryTask(const HttpRequest& request,HttpResponse& response):resp(response){
                data = request.query();
                userCookies = request.getHeader("Cookie");
                userAgent = request.getHeader("User-Agent");
                userIp = request.getHeader("X-Forwarded-For");
                referer = request.getHeader("Referer");
                isPost = request.method()== HttpRequest::Method::kPost;
                needLog = true;
                updateThreadData();
            }

            void updateThreadData();

            virtual ~AbstractQueryTask(){
                conn.reset();
            }

            /**
             * 过滤安全参数
             */
            virtual void filterParamMapSafe(ParamMap& paramMap);

            /**
             * 处理请求的通用逻辑
             * 1.装填log 对象并序列化
             * 2.发送日志
             * 3.准备http response
             */
            void commonLogic(ParamMap& paramMap,protocol::log::LogItem& log,HttpResponse& resp);

            virtual protocol::log::LogPhaseType currentPhase() = 0;

            // 触发的HTTP请求方法
            virtual int reqMethod(){
                return HTTP_REQUEST_GET;
            }

            // 期望http 请求状态
            virtual HttpResponse::HttpStatusCode expectedReqStatus(){
                return HttpResponse::k200Ok;
            }

            virtual void getPostParam(ParamMap& paramMap){}

            // deal with custom bussiness
            virtual void customLogic(ParamMap& paramMap,protocol::log::LogItem& log,HttpResponse& response) = 0;

            // set error detail to response body
            virtual void onError(std::exception& e,HttpResponse& response) = 0;

            void setLogger(const adservice::log::LogPusherPtr& logger){
                serviceLogger = logger;
            }

            void doLog(protocol::log::LogItem& log);

            void operator()();
        protected:
            adservice::types::string userCookies;
            adservice::types::string userAgent;
            adservice::types::string userIp;
            adservice::types::string data;
            adservice::types::string referer;
            bool isPost;
            bool needLog;
            HttpResponse& resp;
            TcpConnectionPtr conn;
            TaskThreadLocal* threadData;
            adservice::log::LogPusherPtr serviceLogger;
        };

    }
}

#endif //ADCORE_ABSTRACT_QUERY_TASK_H
