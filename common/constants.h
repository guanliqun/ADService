//
// Created by guoze.lin on 16/2/15.
//

#ifndef ADCORE_CONSTANTS_H
#define ADCORE_CONSTANTS_H

/**日志类别相关常量**/
// 点击日志生成器名
#define CLICK_SERVICE_LOGGER    "ClickServiceLog"

/**日志引擎相关默认参数*/
#define DEFAULT_KAFKA_BROKER    "192.168.31.147"
#define DEFAULT_KAFKA_TOPIC     "mt-new-click"
#define DEFAULT_KAFKA_KEY       "click"

#define DEFAULT_ALIYUN_PRODUCER_ID      "PID_mtty001"
#define DEFAULT_ALIYUN_TOPIC            "adlog"
#define DEFAULT_ALIYUN_ACCESS_KEY       "5jaQzkjjARFVFUrE"
#define DEFAULT_ALIYUN_SECRET_KEY       "SbFRrY6y1cnSKcdC0QpK1Vkv0QMmTw"

/**ADSelect相关参数*/
// 默认接入节点
#define DEFAULT_ADSELECT_NODE           "123.56.15.234"
// 默认连接端口
#define DEFAULT_ADSELECT_PORT           9200

/**Http 参数相关常量**/
// cookies userId key
#define COOKIES_MTTY_ID         "m1"
// 服务根域名
#define COOKIES_MTTY_DOMAIN     "mtty.com"
// HTTP GET请求
#define HTTP_REQUEST_GET        1
// HTTP POST请求
#define HTTP_REQUEST_POST       0

/**时间相关常量**/
#define DAY_SECOND              86440

/**配置类别常量**/
// 服务器相关配置
#define CONFIG_SERVICE          "ServerConfig"
// 点击模块相关配置
#define CONFIG_CLICK            "ClickConfig"
// 日志相关的配置键
#define CONFIG_LOG              "LogConfig"
// ADSelect组件配置
#define CONFIG_ADSELECT         "ADSelectConfig"

/**配置文件路径常量**/
// 服务配置文件
#define CONFIG_SERVICE_PATH     "conf/service.conf"
// 点击模块配置文件
#define CONFIG_CLICK_PATH       "conf/click.conf"
// 相关日志配置文件
#define CONFIG_LOG_PATH         "conf/log.conf"
// ADSelect相关配置文件
#define CONFIG_ADSELECT_PATH    "conf/adselect.conf"

/**核心逻辑模块常量**/
// 点击逻辑服务屏蔽字
#define MASK_CLICK              0X00000001
// 曝光逻辑服务屏蔽字
#define MASK_SHOW               0X00000002
// 竞价逻辑服务屏蔽字
#define MASK_BID                0X00000004
// 曝光统计模块服务屏蔽字
#define MASK_VIEW               0X00000008
// 跟踪逻辑模块服务屏蔽字
#define MASK_TRACK              0X00000010


/**url 参数常量*/
// 落地URL
#define URL_LANDING_URL         "url"
// referer
#define URL_REFERER             "f"
// 广告位ID
#define URL_ADPLACE_ID          "s"
// 曝光ID
#define URL_EXPOSE_ID           "r"
// 广告主ID
#define URL_ADOWNER_ID          "d"
// 推广计划ID
#define URL_ADPLAN_ID           "t"
// 投放单元ID
#define URL_EXEC_ID             "e"
// 创意ID
#define URL_CREATIVE_ID         "c"
// 平台ID,ADX ID
#define URL_ADX_ID              "x"
// 点击ID
#define URL_CLICK_ID            "h"
// 区域ID
#define URL_AREA_ID             "a"
// 点击区域坐标x
#define URL_CLICK_X             "sx"
// 点击区域坐标y
#define URL_CLICK_Y             "sy"
// ADX 宏
#define URL_ADX_MACRO           "l"
// 出价价格
#define URL_BID_PRICE           "b"
// 成交价格
#define URL_EXCHANGE_PRICE      "p"
// of,曝光类型
#define URL_IMP_OF              "of"


/** 曝光类型 */
// 广告代理曝光
#define OF_DSP                  "0"
// 自有媒体曝光
#define OF_SSP                  "1"

/** HTML模版路径 */
// 曝光模块返回ADX html模板
#define TEMPLATE_SHOW_ADX_PATH      "html/show_adx.html"
// 曝光模块返回SSP html模块
#define TEMPLATE_SHOW_SSP_PATH      "html/show_ssp.html"


/** ADX 广告交换商代码 */
// 淘宝
#define ADX_TANX                1
// 百度
#define ADX_BAIDU               6

/** elasticsearch 相关参数 */
// 投放单ID基数
#define ES_SOLUTION_ID_BASE       1000000
// 创意ID基数
#define ES_BANNER_ID_BASE         5000000
// ES中的索引名
#define ES_INDEX_SOLUTIONS     "solutions"
// ES中的投放类型表
#define ES_DOCUMENT_SOLUTION   "solution"
// ES中的创意类型表
#define ES_DOCUMENT_BANNER     "banner"
// ES中查询格式参数
#define ES_FILTER_FORMAT       "?pretty&filter_path=hits.total,hits.hits._source"

// ES中查询创意的DSL文件路径
#define  ES_QUERY_CREATIVE      "elasticsearch/dsl/query_banner.dsl"


#endif //ADCORE_CONSTANTS_H
