//
// Created by guoze.lin on 16/5/3.
//

#ifndef ADCORE_YOUKU_BIDDING_HANDLER_H
#define ADCORE_YOUKU_BIDDING_HANDLER_H

#include "protocol/base/abstract_bidding_handler.h"


namespace protocol{
    namespace bidding{

        class YoukuBiddingHandler : public AbstractBiddingHandler {
        public:
            /**
             * 从Adx Bid Post请求数据中获取具体的请求信息
             */
            bool parseRequestData(const std::string& data);

            /**
             * 根据Bid 的相关信息对日志进行信息填充
             */
            void fillLogItem(protocol::log::LogItem& logItem);

            /**
             * 根据ADX的请求进行竞价匹配,决定是否接受这个流量,同时设置isBidAccepted
             * @return: true接受流量,false不接受流量
             */
            bool filter(const BiddingFilterCallback& filterCb);

            /**
             * 当接受流量时装配合适的输出
             */
            void match(INOUT HttpResponse& response);

            /**
             * 不接受ADX的流量请求
             */
            void reject(INOUT HttpResponse& response);
        };

    }
}

#endif //ADCORE_YOUKU_BIDDING_HANDLER_H
