//
// Created by guoze.lin on 16/2/2.
//

#include <signal.h>
#include "core_service.h"
#include "utility/utility.h"
#include "protocol/log/log.h"
#include "common/types.h"
#include "core_config_manager.h"
#include "core_adselect_manager.h"
#include "atomic.h"
#include "protocol/baidu/baidu_price.h"
#include "protocol/tanx/tanx_price.h"
#include "protocol/youku/youku_price.h"
#ifdef USE_ENCODING_GZIP
#include "muduo/net/ZlibStream.h"
#endif
#include <exception>

extern adservice::corelogic::CoreModule g_coreService;

long handleShowRequests = 0;
long updateShowRequestsTime = 0;

namespace adservice{

    namespace corelogic{

        using namespace std::placeholders;
        using namespace muduo;
        using namespace muduo::net;
        using namespace adservice::utility::serialize;
        using namespace adservice::utility::cypher;
        using namespace adservice::utility::url;
        using namespace adservice::utility::hash;
        using namespace adservice::utility::file;
        using namespace adservice::adselect;
        using namespace adservice::server;
        using namespace adservice::types;
        using namespace rapidjson;

        void handle_sigsegv(int signal,siginfo_t* siginfo,void* p){
            DebugMessage("segment fault detected,address:",siginfo->si_addr);
            if(g_coreService.use_count()==0)
                exit(0);
            g_coreService->setNeedRestart();
            g_coreService->stop();
        }

        void signal_sigsegv(){
            struct sigaction sa;
            sa.sa_handler = handle_sigsegv;
            sa.sa_flags = SA_SIGINFO;
            sigaction(SIGSEGV, &sa, 0);
        }

        /**
             * 对ADX成交价进行解密
             */
        int decodeAdxExchangePrice(int adx,const std::string& input){
            if(input.empty())
                return 0;
            switch(adx){
                case ADX_TANX:
                    return tanx_price_decode(input);
                case ADX_YOUKU:
                    return youku_price_decode(input);
                case ADX_BAIDU:
                    return baidu_price_decode(input);
                default:
                    return 0;
            }
        }

        /**
         * 将请求参数按需求转换为Log对象
         * 鉴于不同的模块的参数有所差异,所以为了提高parse的速度,可以做parser分发,这是后续优化的点
         */
        void parseObjectToLogItem(ParamMap &paramMap,protocol::log::LogItem &log,const char* allQuery = NULL){ //考虑将临时对象作为static const对象来处理
            char buf[1024];
            adservice::types::string output;
            ParamMap::iterator iter;
            try {
                if ((iter=paramMap.find(URL_LANDING_URL)) != paramMap.end()) { //落地页
                    adservice::types::string &url = iter->second;//paramMap[URL_LANDING_URL];
                    urlDecode_f(url, output, buf);
                    log.adInfo.landingUrl = buf;
                }
                if ((iter=paramMap.find(URL_REFERER)) != paramMap.end()) { //ref
                    adservice::types::string &f = iter->second;//paramMap[URL_REFERER];
                    urlDecode_f(f, output, buf);
                    log.referer = f;
                }
                if ((iter=paramMap.find(URL_ADPLACE_ID)) != paramMap.end()) { //广告位Id
                    adservice::types::string &s = iter->second;//paramMap[URL_ADPLACE_ID];
                    log.adInfo.pid = s;
                    log.adInfo.adxpid = s;
                }
                if ((iter=paramMap.find(URL_EXPOSE_ID)) != paramMap.end()) { //曝光Id
                    adservice::types::string &r = iter->second;//paramMap[URL_EXPOSE_ID];
                    log.adInfo.imp_id = r;
                }
                if ((iter=paramMap.find(URL_ADOWNER_ID)) != paramMap.end()) { //广告主Id
                    adservice::types::string &d = iter->second;//paramMap[URL_ADOWNER_ID];
                    log.adInfo.advId = std::stol(d);
                    log.adInfo.cpid = log.adInfo.advId;
                }
//                if ((iter=paramMap.find(URL_ADPLAN_ID)) != paramMap.end()) { // 推广计划Id
//                    adservice::types::string &t = iter->second;//paramMap[URL_ADPLAN_ID];
//                    log.adInfo.cpid = std::stol(t);
//                }
                if ((iter=paramMap.find(URL_EXEC_ID)) != paramMap.end()) { // 投放单元Id
                    adservice::types::string &e = iter->second;//paramMap[URL_EXEC_ID];
                    log.adInfo.sid = std::stol(e);
                }
                if ((iter=paramMap.find(URL_CREATIVE_ID)) != paramMap.end()) { // 创意Id
                    adservice::types::string &c = iter->second;//paramMap[URL_CREATIVE_ID];
                    log.adInfo.bannerId = std::stol(c);
                }
                if ((iter=paramMap.find(URL_ADX_ID)) != paramMap.end()) { // 平台Id,adxId
                    adservice::types::string &x = iter->second;//paramMap[URL_ADX_ID];
                    log.adInfo.adxid = std::stoi(x);
                }
                if ((iter=paramMap.find(URL_CLICK_ID)) != paramMap.end()) { // clickId
                    adservice::types::string &h = iter->second;//paramMap[URL_CLICK_ID];
                    log.adInfo.clickId = h;
                }
                if ((iter=paramMap.find(URL_AREA_ID)) != paramMap.end()) { //areaId
                    adservice::types::string &a = iter->second;//paramMap[URL_AREA_ID];
                    log.adInfo.areaId = a;
                    int country,province,city;
                    extractAreaInfo(a.c_str(),country,province,city);
                    log.geoInfo.country=country;
                    log.geoInfo.province = province;
                    log.geoInfo.city = city;
                }
                if ((iter=paramMap.find(URL_CLICK_X)) != paramMap.end()){ //点击坐标x
                    adservice::types::string &sx = iter->second;//paramMap[URL_CLICK_X];
                    log.clickx = std::stoi(sx);
                }
                if ((iter=paramMap.find(URL_CLICK_Y)) != paramMap.end()){ //点击坐标y
                    adservice::types::string &sy = iter->second;//paramMap[URL_CLICK_Y];
                    log.clicky = std::stoi(sy);
                }
                if ((iter=paramMap.find(URL_EXCHANGE_PRICE)) != paramMap.end()){ //成交价格
                    adservice::types::string &price = iter->second;//paramMap[URL_EXCHANGE_PRICE];
                    //urlDecode_f(price,output,buffer);
                    log.adInfo.cost = decodeAdxExchangePrice(log.adInfo.adxid,price);
                    log.adInfo.bidPrice = (int)(log.adInfo.cost * AD_OWNER_COST_FACTOR);
                }
            }catch(std::exception& e){
                log.reqStatus = 500;
                LOG_ERROR<<"error:"<<e.what()<<",when processing query "<<allQuery;
            }
        }

        void calculateLogSize(const protocol::log::LogItem& log){
            long size = 0;
            size+=log.userId.length();
            size+=log.userAgent.length();
            size+=sizeof(log.traceId);
            size+=sizeof(log.timeStamp);
            size+=sizeof(log.reqStatus);
            size+=sizeof(log.reqMethod);
            size+=log.referer.length();
            size+=log.path.length();
            size+=log.pageInfo.length();
            size+=log.deviceInfo.length();
            size+=log.jsInfo.length();
            size+=sizeof(log.userInfo.age);
            size+=sizeof(log.userInfo.interest);
            size+=sizeof(log.userInfo.sex);
            size+=sizeof(log.adInfo.bannerId);
            size+=sizeof(log.adInfo.sid);
            size+=sizeof(log.adInfo.advId);
            size+=sizeof(log.adInfo.bidPrice);
            size+=sizeof(log.adInfo.cost);
            size+=sizeof(log.adInfo.cpid);
            size+=log.adInfo.pid.length();
            size+=sizeof(log.adInfo.adxid);
            size+=log.adInfo.adxuid.length();
            size+=log.adInfo.areaId.length();
            size+=sizeof(log.adInfo.cid);
            size+=log.adInfo.clickId.length();
            size+=log.adInfo.imp_id.length();
            size+=log.adInfo.landingUrl.length();
            size+=sizeof(log.adInfo.adxpid);
            size+=sizeof(log.adInfo.adxuid);
            size+=sizeof(log.adInfo.offerPrice);
            size+=sizeof(log.adInfo.mid);
            size+=sizeof(log.geoInfo.city);
            size+=sizeof(log.geoInfo.country);
            size+=sizeof(log.geoInfo.district);
            size+=sizeof(log.geoInfo.province);
            size+=sizeof(log.geoInfo.street);
            size+=log.geoInfo.latitude.length();
            size+=log.geoInfo.longitude.length();
            DebugMessage("log Item,raw size:",size);
        }

        bool operator==(const protocol::log::IPInfo& a,const protocol::log::IPInfo& b){
            return a.ipv4==b.ipv4&&a.proxy==b.proxy;
        }

        bool operator==(const protocol::log::AdInfo& a,const protocol::log::AdInfo& b){
            return a.advId==b.advId && a.adxid == b.adxid && a.areaId == b.areaId && a.bidPrice == b.bidPrice
                    && a.cid == b.cid && a.cost == b.cost && a.clickId == b.clickId && a.cpid == b.cpid
                   && a.bannerId == b.bannerId && a.imp_id == b.imp_id && a.landingUrl == b.landingUrl
                    && a.adxuid == b.adxuid;
        }

        bool operator==(const protocol::log::GeoInfo& a, const protocol::log::GeoInfo& b){
            return a.city == b.city && a.country == b.country && a.district == b.district
                    && a.latitude == b.latitude && a.longitude == b.longitude;
        }

        bool operator==(const protocol::log::LogItem& a,const protocol::log::LogItem& b){
            bool equal = true;
            equal = equal && a.timeStamp == b.timeStamp;
            equal = equal && a.ipInfo == b.ipInfo;
            equal = equal && a.adInfo == b.adInfo;
            equal = equal && a.deviceInfo == b.deviceInfo;
            equal = equal && a.geoInfo == b.geoInfo;
            equal = equal && a.host == b.host;
            equal = equal && a.jsInfo == b.jsInfo;
            equal = equal && a.path == b.path;
            equal = equal && a.referer == b.referer;
            equal = equal && a.logType == b.logType;
            equal = equal && a.reqMethod == b.reqMethod;
            equal = equal && a.reqStatus == b.reqStatus;
            equal = equal && a.userId == b.userId;
            return equal;
        }

        void checkAliEscapeSafe(protocol::log::LogItem &logItem,std::string& input){
            using namespace adservice::utility::escape;
            std::string escape_string = encode4ali(input);
            std::string decode_string = decode4ali(escape_string);
            const char* a = input.c_str();
            const char* b = decode_string.c_str();
            if(input.length()==decode_string.length()){
                bool isSame = true;
                for(int i=0;i<input.length();i++){
                    if(a[i]!=b[i]){
                        isSame = false;
                        break;
                    }
                }
                if(isSame){
                    protocol::log::LogItem parseLog;
                    getAvroObject(parseLog,(uint8_t*)decode_string.c_str(),decode_string.length());
                    if(logItem==parseLog){
                        DebugMessage("parseback check,content equal,escape4ali safe");
                    }
                }else{
                    DebugMessage("decoded string not equal origin,escape4ali not safe");
                }
            }else{
                DebugMessage("decoded string length not equal origin,escape4ali not safe,",input.length()," ",decode_string.length());
            }
        }

        bool checkUserCookies(const adservice::types::string& oldCookies){
            CypherResult128 cypherResult;
            memcpy((void*)cypherResult.bytes,(void*)oldCookies.c_str(),oldCookies.length());
            DecodeResult64 decodeResult64;
            if(!cookiesDecode(cypherResult,decodeResult64)
               || decodeResult64.words[0]<=0
               || decodeResult64.words[0]>time::getCurrentTimeSinceMtty()){
                return false;
            }
            return true;
        }

        /**
         * 处理请求的抽象逻辑
         */
        class AbstractQueryTask{
        public:
            explicit AbstractQueryTask(const TcpConnectionPtr& _conn,const HttpRequest& request):conn(_conn){
                data = request.query();
                userCookies = request.getHeader("Cookie");
                userAgent = request.getHeader("User-Agent");
                userIp = request.getHeader("X-Forwarded-For");
                referer = request.getHeader("Referer");
                needLog = true;
            }

            /**
             * 过滤安全参数
             */
            void filterParamMapSafe(ParamMap& paramMap){
                for(ParamMap::iterator iter = paramMap.begin();iter!=paramMap.end();iter++){
                    iter->second = stringSafeInput(iter->second,URL_LONG_INPUT_PARAMETER);
                }
                paramMap[URL_CREATIVE_ID]=stringSafeNumber(paramMap[URL_CREATIVE_ID]);
                paramMap[URL_SSP_ADX_PID]=stringSafeNumber(paramMap[URL_SSP_ADX_PID]);
                paramMap[URL_SSP_PID]=stringSafeNumber(paramMap[URL_SSP_PID]);
            }

            /**
             * 处理请求的通用逻辑
             * 1.装填log 对象并序列化
             * 2.发送日志
             * 3.准备http response
             */
            void commonLogic(ParamMap& paramMap,protocol::log::LogItem& log,HttpResponse& resp){
                getParam(paramMap,data.c_str()+1);
                filterParamMapSafe(paramMap);
                log.userAgent = userAgent;
                log.logType = currentPhase();
                log.reqMethod = reqMethod();
                log.reqStatus = expectedReqStatus();
                log.timeStamp = utility::time::getCurrentTimeStampUtc();
                log.referer = referer;
                parseObjectToLogItem(paramMap,log,data.c_str()+1);
//                const muduo::net::InetAddress& peerAddr = conn->peerAddress();
//                log.ipInfo.ipv4=peerAddr.ipNetEndian(); //因为现在的服务器是基于ipv4的,所以只需设置ipv4
                log.ipInfo.proxy=userIp;
                adservice::types::string userId = extractCookiesParam(COOKIES_MTTY_ID,userCookies);
                bool needNewCookies = false;
                if(userId.empty()||!checkUserCookies(userId)){
                    CypherResult128 cookiesResult;
                    makeCookies(cookiesResult);
                    log.userId = (char*)cookiesResult.char_bytes;
                    needNewCookies = true;
                }else{
                    log.userId = userId;
                }
                resp.setContentType("text/html");
#ifdef USE_ENCODING_GZIP
                resp.addHeader("Content-Encoding","gzip");
#endif
                //resp.addHeader("Server", "Nginx 1.8.0");
                if(needNewCookies) { //传入的cookies中没有userId,cookies 传出
                    char cookiesString[64];
                    sprintf(cookiesString, "%s=%s;Domain=.%s;Max-Age=2617488000;", COOKIES_MTTY_ID,log.userId.c_str(),COOKIES_MTTY_DOMAIN);//必要时加入Domain
                    resp.addHeader("Set-Cookie", cookiesString);
                }
            }

            virtual protocol::log::LogPhaseType currentPhase() = 0;

            // 触发的HTTP请求方法
            virtual int reqMethod(){
                return HTTP_REQUEST_GET;
            }

            // 期望http 请求状态
            virtual HttpResponse::HttpStatusCode expectedReqStatus(){
                return HttpResponse::k200Ok;
            }

            // deal with custom bussiness
            virtual void customLogic(ParamMap& paramMap,protocol::log::LogItem& log,HttpResponse& response) = 0;

            // set error detail to response body
            virtual void onError(std::exception& e,HttpResponse& response) = 0;

            void doLog(protocol::log::LogItem& log){
                std::shared_ptr<adservice::types::string> logString = std::make_shared<adservice::types::string>();
                writeAvroObject(log, *(logString.get()));
#if defined(USE_ALIYUN_LOG) && defined(UNIT_TEST)
                checkAliEscapeSafe(log,*(logString.get()));
#endif
                // 将日志对象推送到阿里云队列
                CoreModule coreModule = CoreService::getInstance();
                if (coreModule.use_count() > 0)
                    coreModule->getLogger()->push(logString);
            }

            void operator()(){
                try{
                    ParamMap paramMap;
                    protocol::log::LogItem log;
                    HttpResponse resp(false);
                    resp.setStatusCode(expectedReqStatus());
                    commonLogic(paramMap,log,resp);
                    customLogic(paramMap,log,resp);
                    if(needLog)
                        doLog(log);
                    Buffer buf;
                    resp.appendToBuffer(&buf);
                    conn->send(&buf); //这里将异步调用IO线程,进行数据回写
#ifdef USE_SHORT_CONN
                    conn->shutdown(); //假定都是短链接
#endif
                }catch(std::exception& e){
                    HttpResponse resp(false);
                    resp.setStatusCode(HttpResponse::k500ServerError);
                    resp.setStatusMessage("error");
                    resp.setContentType("text/html");
                    //resp.addHeader("Server", "nginx/1.8.0");
                    onError(e,resp);
                    Buffer buf;
                    resp.appendToBuffer(&buf);
                    conn->send(&buf);
                    conn->shutdown();
                }
            }
        protected:
            adservice::types::string userCookies;
            adservice::types::string userAgent;
            adservice::types::string userIp;
            adservice::types::string data;
            adservice::types::string referer;
            bool needLog;
            const TcpConnectionPtr conn;
        };

        /**
         * 处理点击模块逻辑的类
         */
        class HandleClickQueryTask:public AbstractQueryTask{
        public:
            explicit HandleClickQueryTask(const TcpConnectionPtr& _conn,const HttpRequest& request):AbstractQueryTask(_conn,request){
            }

            protocol::log::LogPhaseType currentPhase(){
                return protocol::log::LogPhaseType::CLICK;
            }

            // 期望http 请求状态
            HttpResponse::HttpStatusCode expectedReqStatus(){
                return HttpResponse::k302Redirect;
            }

            void customLogic(ParamMap& paramMap,protocol::log::LogItem& log,HttpResponse& resp){
                if(!log.adInfo.landingUrl.empty()) {
                    resp.setStatusCode(HttpResponse::k302Redirect);
                    resp.addHeader("Location", log.adInfo.landingUrl);
                    resp.setStatusMessage("OK");
                }else{
                    resp.setStatusCode(HttpResponse::k400BadRequest);
                    resp.setStatusMessage("Error,empty landing url");
                }
            }

            void onError(std::exception& e,HttpResponse& resp){
                LOG_ERROR<<"error occured in HandleClickQueryTask:"<<e.what();
                //resp.setBody("error occured in click query");
            }
        };

        /**
         * 处理曝光模块逻辑的类
         */
        class HandleShowQueryTask : public AbstractQueryTask{
        public:
            static int initialized;
            static char showAdxTemplate[1024];
            static char showSspTemplate[1024];
            static void loadTemplates(){
                if(initialized==1||!ATOM_CAS(&initialized,0,1))
                    return;
                loadFile(showAdxTemplate,TEMPLATE_SHOW_ADX_PATH);
                loadFile(showSspTemplate,TEMPLATE_SHOW_SSP_PATH);
            }
        public:
            explicit HandleShowQueryTask(const TcpConnectionPtr& _conn,const HttpRequest& request):AbstractQueryTask(_conn,request){
                loadTemplates();
            }

            protocol::log::LogPhaseType currentPhase(){
                return protocol::log::LogPhaseType::SHOW;
            }

            // 期望http 请求状态
            HttpResponse::HttpStatusCode expectedReqStatus(){
                return HttpResponse::k200Ok;
            }

            bool isShowForSSP(ParamMap& paramMap){
                ParamMap::const_iterator iter = paramMap.find(URL_IMP_OF);
                if(iter==paramMap.end()){
                    return false;
                }else{
                    return iter->second == OF_SSP;
                }
            }

            bool isShowForMobile(ParamMap& paramMap){
                ParamMap::const_iterator iter = paramMap.find(URL_IMP_OF);
                if(iter==paramMap.end()){
                    return false;
                }else{
                    return iter->second == OF_DSP_MOBILE_SHOW || iter->second == OF_DSP_MOBILE_LOG;
                }
            }

            /**
             * 根据OF类型判定是否需要显示创意和纪录日志,http://redmine.mtty.com/redmine/issues/48
             */
            void dspSetParam(ParamMap& paramMap,bool& showCreative,bool& shouldLog){
                ParamMap::const_iterator iter = paramMap.find(URL_IMP_OF);
                if(iter==paramMap.end() || iter->second==OF_DSP){
                    showCreative = true;
                    shouldLog =true;
                }else if(iter->second == OF_DSP_MOBILE_SHOW){
                    showCreative = true;
                    shouldLog = false;
                }else if(iter->second == OF_DSP_MOBILE_LOG){
                    showCreative = false;
                    shouldLog = true;
                }
            }

#define MakeStringValue(s) rapidjson::Value().SetString(s.c_str(),s.length())

#define MakeStringConstValue(s) rapidjson::Value().SetString(s)

#define MakeIntValue(s) rapidjson::Value().SetInt(s)

#define MakeDoubleValue(d) rapidjson::Value().SetDouble(d)

#define MakeBooleanValue(b) rapidjson::Value().SetBool(b)


            void prepareMtAdInfoForDSP(rapidjson::Document& mtAdInfo,ParamMap& paramMap,rapidjson::Value& result){
                rapidjson::Document::AllocatorType& allocator = mtAdInfo.GetAllocator();
                mtAdInfo.SetObject();
                mtAdInfo.AddMember("pid",MakeStringValue(paramMap[URL_ADPLACE_ID]),allocator);
                mtAdInfo.AddMember("width",MakeIntValue(json::getField(result,"width",250)),allocator);
                mtAdInfo.AddMember("height",MakeIntValue(json::getField(result,"height",300)),allocator);
                mtAdInfo.AddMember("impid",MakeStringValue(paramMap[URL_EXPOSE_ID]),allocator);
                mtAdInfo.AddMember("advid",MakeStringValue(paramMap[URL_ADOWNER_ID]),allocator);
                mtAdInfo.AddMember("unid",MakeStringValue(paramMap[URL_ADX_ID]),allocator);
                mtAdInfo.AddMember("plid",MakeStringConstValue(""),allocator);
                mtAdInfo.AddMember("gpid",MakeStringValue(paramMap[URL_EXEC_ID]),allocator);
                mtAdInfo.AddMember("cid",MakeStringValue(paramMap[URL_CREATIVE_ID]),allocator);
                mtAdInfo.AddMember("arid",MakeStringValue(paramMap[URL_AREA_ID]),allocator);
                mtAdInfo.AddMember("ctype",MakeIntValue(json::getField(result,"banner_type",1)),allocator);
                mtAdInfo.AddMember("xcurl",MakeStringValue(paramMap[URL_ADX_MACRO]),allocator);
                mtAdInfo.AddMember("tview",MakeStringConstValue(""),allocator);
                rapidjson::Value mtls(kArrayType);
                rapidjson::Value mtlsObj(kObjectType);
                mtlsObj.AddMember("p0",MakeStringValue(json::getField(result,"material_url","")),allocator);
                mtlsObj.AddMember("p1",MakeStringValue(json::getField(result,"click_url","")),allocator);
                mtlsObj.AddMember("p2",MakeStringConstValue("000"),allocator);
                mtlsObj.AddMember("p3",MakeIntValue(json::getField(result,"width",250)),allocator);
                mtlsObj.AddMember("P4",MakeIntValue(json::getField(result,"height",300)),allocator);
                mtlsObj.AddMember("p5",MakeStringConstValue(""),allocator);
                mtlsObj.AddMember("p6",MakeStringConstValue(""),allocator);
                mtlsObj.AddMember("p7",MakeStringConstValue(""),allocator);
                mtlsObj.AddMember("p8",MakeStringConstValue(""),allocator);
                mtls.PushBack(mtlsObj.Move(),allocator);
                mtAdInfo.AddMember("mtls",mtls.Move(),mtAdInfo.GetAllocator());
            }

            int fillHtmlFixedParam(ParamMap& paramMap,const char* html,const char* templateFmt,char* buffer,int bufferSize){
                char mtAdInfo[2048];
                char adxbuffer[1024];
                std::string adxMacro;
                urlDecode_f(paramMap[URL_ADX_MACRO],adxMacro,adxbuffer);
                adxMacro+=ADX_MACRO_SUFFIX;
                //这里因为使用了C++的字符串模板进行参数绑定,而获取的模板中有可能带有混淆字符,所以需要分解处理
                //更好的解决方法还是使用json parse的方式,不过要求html的json格式按照RFC规范
                char* mtlsPos = (char*)strstr(html,"mtls");
                char backupChar = '\0';
                if(mtlsPos!=NULL){
                    backupChar = *mtlsPos;
                    *mtlsPos = '\0';
                }
                int len=snprintf(mtAdInfo,2048,html,paramMap[URL_ADPLACE_ID].c_str(),paramMap[URL_EXPOSE_ID].c_str(),paramMap[URL_ADX_ID].c_str(),
                                "",paramMap[URL_EXEC_ID].c_str(),paramMap[URL_AREA_ID].c_str(),adxMacro.c_str());
                if(mtlsPos!=NULL){
                    *mtlsPos = backupChar;
                    strncpy(mtAdInfo+len,mtlsPos,2048-len);
                }
                tripslash(mtAdInfo);
                len = snprintf(buffer,bufferSize,templateFmt,mtAdInfo);
                return len;
            }

            int fillHtmlFixedParam(rapidjson::Value& solution,rapidjson::Value& adplace,rapidjson::Value& banner,ParamMap& paramMap,const char* html,
                                   const char* templateFmt,char* buffer,int bufferSize){
                char mtAdInfo[2048];
                char* mtlsPos = (char*)strstr(html,"mtls");
                char backupChar = '\0';
                if(mtlsPos!=NULL){
                    backupChar = *mtlsPos;
                    *mtlsPos = '\0';
                }
                int len=snprintf(mtAdInfo,2048,html,to_string(adplace["pid"].GetInt()).c_str(),cypher::randomId(5).c_str(),to_string(adplace["adxid"].GetInt()).c_str(),
                            "",to_string(solution["sid"].GetInt()).c_str(),"0086-ffff-ffff","");
                if(mtlsPos!=NULL){
                    *mtlsPos = backupChar;
                    strncpy(mtAdInfo+len,mtlsPos,2048-len);
                }
                tripslash(mtAdInfo);
                len = snprintf(buffer,bufferSize,templateFmt,paramMap["callback"].c_str(),mtAdInfo);
                return len;
            }

            void fillHtmlUnFixedParam(rapidjson::Document& mtAdInfo,ParamMap& paramMap,rapidjson::Value& result){
                const std::string binder="%s";
                if(binder == mtAdInfo["pid"].GetString()){
                    mtAdInfo["pid"] = MakeStringValue(paramMap[URL_ADPLACE_ID]);
                }
                if(binder == mtAdInfo["width"].GetString()){
                    mtAdInfo["width"] = MakeIntValue(json::getField(result,"width",0));
                }
                if(binder == mtAdInfo["height"].GetString()){
                    mtAdInfo["height"] = MakeIntValue(json::getField(result,"height",0));
                }
                if(binder == mtAdInfo["impid"].GetString()) {
                    mtAdInfo["impid"] = MakeStringValue(paramMap[URL_EXPOSE_ID]);
                }
                if(binder == mtAdInfo["advid"].GetString()) {
                    mtAdInfo["advid"] = MakeStringValue(paramMap[URL_ADOWNER_ID]);
                }
                if(binder == mtAdInfo["unid"].GetString()) {
                    mtAdInfo["unid"]  = MakeStringValue(paramMap[URL_ADX_ID]);
                }
                if(binder == mtAdInfo["plid"].GetString()) {
                    mtAdInfo["plid"] =  MakeStringConstValue("");
                }
                if(binder == mtAdInfo["gpid"].GetString()) {
                    mtAdInfo["gpid"] = MakeStringValue(paramMap[URL_EXEC_ID]);
                }
                if(binder == mtAdInfo["cid"].GetString()) {
                    mtAdInfo["cid"] = MakeStringValue(paramMap[URL_CREATIVE_ID]);
                }
                if(binder == mtAdInfo["arid"].GetString()) {
                    mtAdInfo["arid"] = MakeStringValue(paramMap[URL_AREA_ID]);
                }
                if(binder == mtAdInfo["ctype"].GetString()) {
                    mtAdInfo["ctype"] = MakeIntValue(json::getField(result, "banner_type", 1));
                }
                if(binder == mtAdInfo["xcurl"].GetString()) {
                    mtAdInfo["xcurl"] = MakeStringValue(paramMap[URL_ADX_MACRO]);
                }
                if(binder == mtAdInfo["tview"].GetString()){
                    mtAdInfo["tview"] = MakeStringConstValue("");
                }
            }

            void tripslash(char* str){
                char* p1 = str,*p2=p1;
                while(*p2!='\0'){
                    if(*p2=='\\'&&p2[1]=='\''){
                        p2++;
                    }
                    *p1++ = *p2++;
                }
                *p1='\0';
            }

            rapidjson::Value& selectBannerFromBannerGroup(std::map<int,std::vector<rapidjson::Value*>>& bannerMap,int bgid){
                std::vector<rapidjson::Value*>& banners = bannerMap[bgid];
                int r = rng::randomInt()%banners.size();
                return *(banners[r]);
            }

            rapidjson::Value& bestBannerFromBannerGroup(std::map<int,std::vector<rapidjson::Value*>>& bannerMap,int bgid){
                std::vector<rapidjson::Value*>& banners = bannerMap[bgid];
                int idx[50];
                int idxCnt=0,i=0;
                double maxScore = -1.0;
                typedef std::vector<rapidjson::Value*>::iterator Iter;
                for(Iter iter = banners.begin();iter!=banners.end();iter++,i++){
                    rapidjson::Value& banner = *(*iter);
                    double ecpm = banner["offerprice"].GetDouble();
                    if(ecpm>maxScore){
                        idxCnt = 0;
                        idx[idxCnt++] = i;
                        maxScore = ecpm;
                    }else if(ecpm==maxScore){
                        idx[idxCnt++] = i;
                    }
                }
                int r = rng::randomInt()%idxCnt;
                return *(banners[r]);
            }

            double calcSolutionScore(int priceType,double ecpm){
                return (10-priceType)*10000+ecpm;
            }

            double getECPMFromSolutionScore(double score,int priceType){
                return score - (10 - priceType)*10000;
            }

            void sortSolutionScore(int* idx,double* scores,int size){
                std::sort<int*>(idx,idx+size,[scores](const int& a,const int&b)->bool{
                        return scores[a] > scores[b];
                });
            }

            int randomSolution(double totalWeight,double* accWeights,int size){
                double r = rng::randomDouble() * totalWeight;
                int l=0,h=size-1;
                while(l<=h){
                    int mid = l+((h-l)>>1);
                    if(r<=accWeights[mid])
                        h = mid-1;
                    else
                        l = mid+1;
                }
                assert(l>=0&&l<size);
                return l;
            }

            double calcBidPrice(int priceType,double ecpm1,double ecpm2,double basePrice,double offerPrice){
                switch(priceType){
                    case PRICETYPE_CPD:
                    case PRICETYPE_CPM:
                    case PRICETYPE_CPC:
                        return 0;
                    case PRICETYPE_RCPC: //offerprice
                        return std::max(offerPrice,basePrice);
                    case PRICETYPE_RTB:
                    case PRICETYPE_RRTB_CPM:
                    case PRICETYPE_RRTB_CPC:
                        if(ecpm2<0){ //只有一个出价
                            return std::max(ecpm1,basePrice);
                        }else{ // 第二高价加一毛钱
                            return std::max(ecpm2+0.1,basePrice);
                        }
                }
                return 0;
            }

            void customLogic(ParamMap& paramMap,protocol::log::LogItem& log,HttpResponse& response){
                bool isSSP = isShowForSSP(paramMap);
                const char* templateFmt = isSSP?showSspTemplate:showAdxTemplate;
                //连接ADSelect
                AdSelectManager& adselect = AdSelectManager::getInstance();
                int seqId = 0;
                CoreModule coreModule = CoreService::getInstance();
                if(coreModule.use_count()>0){
                    seqId = coreModule->getExecutor().getThreadSeqId();
                }
                std::string respBody;
                if(isSSP){//SSP
                    std::string& queryPid = paramMap[URL_SSP_PID];
                    bool isAdxPid = false;
                    if(queryPid.empty()){
                        queryPid = paramMap[URL_SSP_ADX_PID];
                        if(queryPid.empty()){ //URL 参数有问题
                            DebugMessageWithTime("in show module,ssp url pid is empty");
                            return;
                        }
                        isAdxPid = true;
                    }
                    rapidjson::Document esResp(rapidjson::kObjectType);
                    rapidjson::Value& result = adselect.queryAdInfoByPid(seqId,queryPid,esResp,isAdxPid);
                    if(!result.IsArray()||result.Empty()){ //正常情况下应该刷出solution和banner以及相应的高级出价器列表
                        log.adInfo.pid = isAdxPid?"0":queryPid;
                        log.adInfo.adxpid = isAdxPid?queryPid:"0";
                        log.reqStatus = 500;
                        return ;
                    }
                    //从返回结果中整理数据
                    rapidjson::Value& adplace = esResp["adplace"];
                    rapidjson::Value* solutions[100];
                    int solCnt = 0;
                    std::map<int,std::vector<rapidjson::Value*>> bannerMap;
                    rapidjson::Value* superPricer =NULL;
                    for(int i=0;i<result.Size();i++){
                        const char* type = result[i]["_type"].GetString();
                        if(!strcmp(type,ES_DOCUMENT_SOLUTION)){ //投放单
                            solutions[solCnt++]=&(result[i]["_source"]);
                        }else if(!strcmp(type,ES_DOCUMENT_BANNER)){ // 创意
                            rapidjson::Value& banner = result[i]["_source"];
                            int bgid = banner["bgid"].GetInt();
                            if(bannerMap.find(bgid)==bannerMap.end()){
                                bannerMap.insert(std::make_pair(bgid,std::vector<rapidjson::Value*>()));
                            }
                            std::vector<rapidjson::Value*>& bannerVec = bannerMap[bgid];
                            bannerVec.push_back(&banner);
                        }else if(!strcmp(type,ES_DOCUMENT_ES_ADPLACE)){ //高级出价器,一个广告位的高级出价器有且只能有一个
                            superPricer = &(result[i]["_source"]);
                        }
                    }
                    if(solCnt==0){ //失败
                        log.adInfo.pid = isAdxPid?"0":queryPid;
                        log.adInfo.adxpid = isAdxPid?queryPid:"0";
                        log.reqStatus = 500;
                        return ;
                    }
                    //计算投放单ecpm,并计算score进行排序
                    int solIdx[100];
                    double finalOfferPrice[100];
                    double solScore[100];
                    double top1Ecpm = -1,top2Ecpm = -1;
                    double hourPercent = getCurrentHourPercent();
                    for(int i=0;i<solCnt;i++){
                        solIdx[i] = i;
                        rapidjson::Value& solution = *(solutions[i]);
                        double offerprice = solution["offerprice"].GetDouble();
                        double ctr = solution["ctr"].GetDouble();
                        if(superPricer && (*superPricer)["sid"].GetInt()==solution["sid"].GetInt()){
                            offerprice+=(*superPricer)["offerprice"].GetDouble();
                            ctr+=(*superPricer)["ctr"].GetDouble();
                        }
                        int bgid = solution["bgid"].GetInt();
                        rapidjson::Value& banner = bestBannerFromBannerGroup(bannerMap,bgid);
                        offerprice += banner["offerprice"].GetDouble();
                        ctr += banner["ctr"].GetDouble();
                        double ecpm = offerprice * ctr * hourPercent;
                        finalOfferPrice[i] = offerprice;
                        int priceType = solution["pricetype"].GetInt();
                        solScore[i] = calcSolutionScore(priceType,ecpm);
                        if(ecpm>top1Ecpm){
                            top1Ecpm = ecpm;
                            top2Ecpm = top1Ecpm;
                        }
                    }
                    sortSolutionScore(solIdx,solScore,solCnt);
                    //按排序概率展示
                    double totalRate = 0;
                    double cpdRate = 0;
                    double accRate[100];
                    for(int i=0;i<solCnt;i++){
                        rapidjson::Value& solution = *(solutions[solIdx[i]]);
                        int priceType = solution["pricetype"].GetInt();
                        double rate = solution["rate"].GetDouble();
                        totalRate+=rate;
                        accRate[i] = totalRate;
                        if(priceType == PRICETYPE_CPD){
                            cpdRate+=rate;
                        }
                    }
                    if(cpdRate>100){
                        totalRate = cpdRate;
                    }else if(cpdRate>0){
                        totalRate = 100;
                    }
                    int finalSolutionIdx = randomSolution(totalRate,accRate,solCnt);
                    rapidjson::Value& finalSolution = *(solutions[solIdx[finalSolutionIdx]]);
                    rapidjson::Value& banner = bestBannerFromBannerGroup(bannerMap,finalSolution["bgid"].GetInt());
                    double offerPrice = finalOfferPrice[solIdx[finalSolutionIdx]];
                    //筛选出最后结果
                    if(!banner.HasMember("html")){ //SSP没刷出广告,属于错误的情况
                        log.adInfo.bannerId = 0;
                        log.reqStatus = 500;
                        return ;
                    }
                    log.adInfo.bannerId = banner["bid"].GetInt();
                    log.adInfo.advId = banner["advid"].GetInt();
                    log.adInfo.adxid = adplace["adxid"].GetInt();
                    log.adInfo.pid = to_string(adplace["pid"].GetInt());
                    log.adInfo.sid = finalSolution["sid"].GetInt();
                    int finalPriceType = finalSolution["pricetype"].GetInt();
                    double basePrice = adplace["baseprice"].GetDouble();
                    log.adInfo.bidPrice = (int)calcBidPrice(finalPriceType,top1Ecpm,top2Ecpm,basePrice,offerPrice);
                    log.adInfo.cost = adplace["costprice"].GetInt();
                    const char* tmp = banner["html"].GetString();
                    //返回结果
                    char buffer[4096];
                    int len = fillHtmlFixedParam(finalSolution,adplace,banner,paramMap,tmp,templateFmt,buffer,sizeof(buffer));
                    respBody = std::string(buffer,buffer+len);
                }else {//DSP of=0,of=2,of=3
                    bool showCreative = true;
                    dspSetParam(paramMap,showCreative,needLog);
                    rapidjson::Document esResp(rapidjson::kObjectType);
                    rapidjson::Value &result = adselect.queryCreativeByIdCache(seqId, paramMap[URL_CREATIVE_ID], esResp);
                    if (!result.HasMember("html")) {
                        log.adInfo.bannerId = 0;
                        log.reqStatus = HttpResponse::k500ServerError;
                        return;
                    }
                    if(showCreative) { //需要显示创意
                        const char *tmp = result["html"].GetString();
                        char buffer[4096];
                        DebugMessage("template",tmp);
                        int len = fillHtmlFixedParam(paramMap, tmp, templateFmt, buffer,sizeof(buffer));
                        respBody = std::string(buffer, buffer + len);
                    }
                }
#ifdef USE_ENCODING_GZIP
                Buffer body;
                muduo::net::ZlibOutputStream zlibOutStream(&body);
                zlibOutStream.write(respBody);
                zlibOutStream.finish();
                response.setBody(body.retrieveAllAsString());
#else
                response.setBody(respBody);
#endif
                response.addHeader("Pragma", "no-cache");
                response.addHeader("Cache-Control", "no-cache,no-store;must-revalidate");
                response.addHeader("P3p",
                                   "CP=\"CURa ADMa DEVa PSAo PSDo OUR BUS UNI PUR INT DEM STA PRE COM NAV OTC NOI DSP COR\"");
                handleShowRequests++;
                if (handleShowRequests % 10000 == 1) {
                    int64_t todayStartTime = time::getTodayStartTime();
                    if (updateShowRequestsTime < todayStartTime) {
                        handleShowRequests = 1;
                        updateShowRequestsTime = todayStartTime;
                    } else {
                        DebugMessageWithTime("handleShowRequests:", handleShowRequests);
                    }
                }
            }

            void onError(std::exception& e,HttpResponse& resp){
                LOG_ERROR<<"error occured in HandleShowQueryTask:"<<e.what()<<",query:"<<data;
                //resp.setBody("error occured in show query");
            }
        };

        int HandleShowQueryTask::initialized = 0;
        char HandleShowQueryTask::showAdxTemplate[1024];
        char HandleShowQueryTask::showSspTemplate[1024];

        CoreModule CoreService::getInstance(){
            return g_coreService;
        }

        void CoreService::start(){
            executor.start();
            clickLogger->start();
            server->start();
            loop.loop();
        }

        void setLogLevel(int logLevel){
            switch(logLevel){
                case 1:
                    muduo::Logger::setLogLevel(muduo::Logger::LogLevel::DEBUG);
                    break;
                case 2:
                    muduo::Logger::setLogLevel(muduo::Logger::LogLevel::INFO);
                    break;
                case 3:
                    muduo::Logger::setLogLevel(muduo::Logger::LogLevel::WARN);
                    break;
                case 4:
                    muduo::Logger::setLogLevel(muduo::Logger::LogLevel::ERROR);
                    break;
                case 5:
                    muduo::Logger::setLogLevel(muduo::Logger::LogLevel::FATAL);
                    break;
                default:
                    break;
            }
        }

        void onConfigChange(const std::string& type,void* newData,void* oldData){
            DebugMessageWithTime("config ",type," modified");
            if(g_coreService.use_count()==0)
                return;
            // 简单粗暴地重启服务
            g_coreService->setNeedRestart();
            g_coreService->stop();
        }

        void CoreService::init() {
            ConfigManager& configManager = ConfigManager::getInstance();
            ServerConfig* serverConfig = (ServerConfig*)configManager.get(CONFIG_SERVICE);
            int httpThreads = serverConfig->coreHttpThreads;
            int port = serverConfig->corePort;
            setLogLevel(serverConfig->loggingLevel);
            //点击逻辑初始化相关,==现在的实际情况是所有逻辑都共用同一个logger,囧
            if(serverConfig->runClick) {
                ClickConfig* clickConfig = (ClickConfig*)configManager.get(CONFIG_CLICK);
                clickLogger = adservice::log::LogPusher::getLogger(CLICK_SERVICE_LOGGER,
                                                                   clickConfig->clickLoggerThreads,
                                                                   !clickConfig->clickLogRemote);
                httpThreads = std::max(httpThreads,clickConfig->httpthreads);
                configManager.registerOnChange(CONFIG_CLICK,std::bind(&onConfigChange,CONFIG_CLICK,_1,_2));
            }
            // 初始化http server
            muduo::net::InetAddress addr(static_cast<uint16_t>(port));
            server = std::make_shared<CoreHttpServer>(&loop,addr,"mtty::core_service");
            server->setHttpCallback(std::bind(&CoreService::onRequest,this,_1,_2,_3));
            server->setThreadNum(httpThreads);
            configManager.registerOnChange(CONFIG_SERVICE,std::bind(&onConfigChange,CONFIG_SERVICE,_1,_2));
            configManager.registerOnChange(CONFIG_LOG,std::bind(&onConfigChange,CONFIG_LOG,_1,_2));
            configManager.registerOnChange(CONFIG_ADSELECT,std::bind(&onConfigChange,CONFIG_ADSELECT,_1,_2));
        }

        struct ParamPack{
                const TcpConnectionPtr conn;
                const adservice::types::string data;
                const adservice::types::string cookies;
                ParamPack(const TcpConnectionPtr& con,const adservice::types::string& d,const adservice::types::string& c):
                        conn(con),data(d),cookies(c){}
        };


        void doClick(CoreService* service,const TcpConnectionPtr& conn,const HttpRequest& req,bool isClose){
            try {
                //DebugMessage("in request c,query=", req.query());
                service->getExecutor().run(std::bind(HandleClickQueryTask(conn,req)));
            }catch(std::exception &e){
                LOG_ERROR<<"error occured in ClickService::onRequest"<<e.what();
            }
        }

        void doShow(CoreService* service,const TcpConnectionPtr& conn,const HttpRequest& req,bool isClose){
            try{
                //DebugMessage("in request v,query=",req.query());
                service->getExecutor().run(std::bind(HandleShowQueryTask(conn,req)));
            }catch(std::exception &e){
                LOG_ERROR<<"error occured in ClickService::onRequest"<<e.what();
            }
        }

        void CoreService::onRequest(const TcpConnectionPtr& conn,const HttpRequest& req, bool isClose) {
            //todo:改成table dispatcher
            if(req.path().length()>URL_LONG_REQUEST_THRESH){
                DebugMessage("Received Long Request,",req.path().length(),",input:",req.path());
            }
            if (req.path() == "/v" || req.path() == "/s") { //show
                doShow(this,conn,req,isClose);
            } else if(req.path() == "/c"){ //click
                doClick(this,conn,req,isClose);
            } else if(req.path() == "/jt.html"){ //http健康检查
                HttpResponse resp(false);
                resp.setStatusCode(HttpResponse::k200Ok);
                Buffer buf;
                resp.appendToBuffer(&buf);
                conn->send(&buf);
#ifdef USE_SHORT_CONN
                conn->shutdown();
#endif
            }
            else // 404
            {
                DebugMessage("req.path() not match target!",req.path());
                HttpResponse resp(isClose);
                resp.setStatusCode(HttpResponse::k404NotFound);
                resp.setStatusMessage("Not Found");
                Buffer buf;
                resp.appendToBuffer(&buf);
                conn->send(&buf);
#ifdef USE_SHORT_CONN
                conn->shutdown();
#endif
            }

        }

    }

}
