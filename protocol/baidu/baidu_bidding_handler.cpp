//
// Created by guoze.lin on 16/5/3.
//

#include "baidu_bidding_handler.h"
#include "utility/utility.h"

namespace protocol {
    namespace bidding {

        using namespace protocol::Baidu;
        using namespace adservice::utility::serialize;

        inline int max(const int& a,const int& b){
            return a>b?a:b;
        }

        bool BaiduBiddingHandler::parseRequestData(const std::string& data){
            bidRequest.Clear();
            return getProtoBufObject(bidRequest,data);
        }

        void BaiduBiddingHandler::fillLogItem(protocol::log::LogItem &logItem) {
            logItem.reqStatus = 200;
            logItem.userAgent = bidRequest.user_agent();
            logItem.ipInfo.proxy = bidRequest.ip();
            if(isBidAccepted){
                if(bidRequest.has_mobile()){
                    const BidRequest_Mobile& mobile = bidRequest.mobile();
                    logItem.deviceInfo = mobile.DebugString();
                }
                logItem.adInfo.advId = adInfo.advId;
                logItem.adInfo.adxid = adInfo.adxid;
                logItem.adInfo.adxpid = adInfo.adxpid;
                logItem.adInfo.adxuid = adInfo.adxuid;
                logItem.adInfo.bannerId = adInfo.bannerId;
                logItem.adInfo.cid = adInfo.cid;
                logItem.adInfo.mid = adInfo.mid;
                logItem.adInfo.cpid = adInfo.cpid;
                logItem.adInfo.offerPrice = adInfo.offerPrice;
            }
        }

        bool BaiduBiddingHandler::filter(const BiddingFilterCallback& filterCb){
            if(bidRequest.is_ping()||bidRequest.is_test()){
                return bidFailedReturn();
            }
            //从BID Request中获取请求的广告位信息,目前只取第一个
            const BidRequest_AdSlot& adSlot = bidRequest.adslot(0);
            long pid = adSlot.ad_block_key();
            AdSelectCondition queryCondition;
            queryCondition.pid = std::to_string(pid);
            if(!filterCb(this,queryCondition)){
                return bidFailedReturn();
            }
            isBidAccepted = false;
            return false;
        }

        void BaiduBiddingHandler::buildBidResult(const SelectResult &result) {
            bidResponse.Clear();
            bidResponse.set_id(bidRequest.id());
            bidResponse.clear_ad();
            BidResponse_Ad* adResult = bidResponse.add_ad();
            rapidjson::Value& finalSolution = *(result.finalSolution);
            rapidjson::Value& adplace = *(result.adplace);
            rapidjson::Value& banner = *(result.banner);
            int advId = finalSolution["advid"].GetInt();
            const BidRequest_AdSlot& adSlot = bidRequest.adslot(0);
            int maxCpmPrice = max(result.bidPrice,adSlot.minimum_cpm());
            adResult->set_max_cpm(maxCpmPrice);
            adResult->set_advertiser_id(advId);
            adResult->set_creative_id(banner["bid"].GetInt());
            adResult->set_height(banner["height"].GetInt());
            adResult->set_width(banner["width"].GetInt());
            adResult->set_sequence_id(adSlot.sequence_id());
            //缓存最终广告结果
            adInfo.advId = advId;
            adInfo.adxid = ADX_BAIDU;
            adInfo.adxpid = adplace["adxpid"].GetInt();
            adInfo.adxuid = bidRequest.baidu_user_id();
            adInfo.bannerId = banner["bid"].GetInt();
            adInfo.cid = adplace["cid"].GetInt();
            adInfo.mid = adplace["mid"].GetInt();
            adInfo.cpid = adInfo.advId;
            adInfo.offerPrice = maxCpmPrice;
        }

        void BaiduBiddingHandler::match(HttpResponse &response) {
            std::string result;
            if(!writeProtoBufObject(bidResponse,&result)){
                DebugMessageWithTime("failed to write protobuf object in BaiduBiddingHandler::match");
                reject(response);
                return;
            }
            response.setStatusCode(HttpResponse::k200Ok);
            response.setBody(result);
        }

        void BaiduBiddingHandler::reject(HttpResponse &response) {
            bidResponse.Clear();
            bidResponse.set_id(bidRequest.id());
            std::string result;
            if(!writeProtoBufObject(bidResponse,&result)){
                DebugMessageWithTime("failed to write protobuf object in BaiduBiddingHandler::reject");
                return;
            }
            response.setStatusCode(HttpResponse::k200Ok);
            response.setBody(result);
        }

    }
}