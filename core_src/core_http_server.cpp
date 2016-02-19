//
// Created by guoze.lin on 16/2/14.
//

#include <functional>
#include <boost/shared_array.hpp>
#include "core_http_server.h"
#include "muduo/base/Timestamp.h"
#include "muduo/base/Types.h"
#include "muduo/net/http/HttpContext.h"
#include "functions.h"



namespace adservice{
    namespace server{

        using namespace muduo::net;

        void CoreHttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req){
          const muduo::string& connection = req.getHeader("Connection");
          bool close = connection == "close" ||
            (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
          httpCallback_(conn,req, &close);
        }

        bool requestParser(HttpContext* context,Buffer* buf,muduo::Timestamp receiveTime){
            bool ok = true;
            bool hasMore = true;
            size_t contentReadSize = 0;
            size_t contentLength = 0;
            HttpRequest& request_ = context->request();
            HttpContext::HttpRequestParseState& state_ = context->getState();
            boost::shared_array<char>& postdata_ = context->getPostData();
            while (hasMore)
            {
                if (state_ == HttpContext::kExpectRequestLine)
                {
                    const char* crlf = buf->findCRLF();
                    if (crlf)
                    {
                        ok = context->processRequestLine(buf->peek(), crlf);
                        if (ok)
                        {
                            request_.setReceiveTime(receiveTime);
                            buf->retrieveUntil(crlf + 2);
                            state_ = HttpContext::kExpectHeaders;
                        }
                        else
                        {
                            hasMore = false;
                        }
                    }
                    else
                    {
                        hasMore = false;
                    }
                }
                else if (state_ == HttpContext::kExpectHeaders)
                {
                    const char* crlf = buf->findCRLF();
                    if (crlf)
                    {
                        const char* colon = std::find(buf->peek(), crlf, ':');
                        if (colon != crlf)
                        {
                            request_.addHeader(buf->peek(), colon, crlf);
                        }
                        else
                        {
                            // empty line, end of header
                            // FIXME:
                            // LGZADD:
                            if(request_.method() == HttpRequest::kPost) {
                                state_ = HttpContext::kExpectBody;
                                muduo::string v = request_.getHeader("Content-Length");
                                contentLength = atoi(v.c_str());
//                                DebugMessage("find post,Content-Length:",contentLength);
                                if(v.empty() || contentLength<=0){
                                    state_ = HttpContext::kGotAll;
                                    hasMore = false;
                                }else{
                                    postdata_.reset(new char[contentLength+1]);
                                }
                            }else {
                                state_ = HttpContext::kGotAll;
                                hasMore = false;
                            }
                        }
                        buf->retrieveUntil(crlf + 2);
                    }
                    else
                    {
                        hasMore = false;
                    }
                }
                else if (state_ == HttpContext::kExpectBody)
                {
                    // FIXME:
                    if(buf->readableBytes()>0) {
                        ::memcpy(postdata_.get()+contentReadSize,buf->peek(),buf->readableBytes());
                        contentReadSize += buf->readableBytes();
                        DebugMessage("received body size:",contentReadSize);
                    }
                    if(contentReadSize>=contentLength) {
                        hasMore = false;
                        contentReadSize = 0;
                        request_.setQuery(postdata_.get(),postdata_.get()+contentLength);
                        DebugMessage("copy querystring length:",request_.query().length());
                        state_ = HttpContext::kGotAll;
                    }
                    buf->retrieveUntil(buf->peek()+buf->readableBytes());
                }
            }

            return ok;
        }

        using namespace std::placeholders;

        void CoreHttpServer::onConnection(const TcpConnectionPtr& conn)
        {
            if (conn->connected())
            {
                conn->setContext(HttpContext());
                HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());
                context->setRequestParser(std::bind(&requestParser,context,_1,_2));
            }
        }

    }
}