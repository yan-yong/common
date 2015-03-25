#include "HttpClient.hpp"
#include "utility/net_utility.h"

void HandleResult(boost::shared_ptr<HttpClient::FetchResult> result)
{
    HttpFetcherResponse* resp = result->resp_;
    struct addrinfo * ai = (struct addrinfo *)result->contex_;
    char ip_str[100];
    uint16_t port;
    get_addr_string(ai->ai_addr, ip_str, 100, port);
    if(result->error_.error_num() == RS_OK)
    {
        LOG_ERROR("===== %s %hu\n", ip_str, port);
    }
    else
    {
        LOG_ERROR("## throw %s %hu\n", ip_str, port);
    }
    freeaddrinfo(ai);
}

int main()
{
    HttpClient http_client;
    http_client.Open();
    http_client.SetResultCallback(HandleResult);
    http_client.SetDefaultServConfig(ServChannel::CONCURENCY_NO_LIMIT, 0.8, 0, 10);
    Fetcher::Params fetch_params;
    fetch_params.conn_timeout.tv_sec  = 10;
    fetch_params.conn_timeout.tv_usec = 0;
    fetch_params.rx_speed_max = 0;
    fetch_params.max_connecting_cnt = 200000;
    fetch_params.socket_rcvbuf_size = 8192;
    fetch_params.socket_sndbuf_size = 8192;
    http_client.SetFetcherParams(fetch_params);

    uint16_t general_port [] = {80, 8080, 3128, 8118, 808};
    for(int i = 0; i < 255; i++)
        for(int j = 0; j < 255; j++)
            for(unsigned k = 0; k < sizeof(general_port)/sizeof(*general_port); k++) 
            {
                uint16_t port = general_port[k];
                char ip_str[200];
                snprintf(ip_str, 200, "36.250.%d.%d", i, j);
                struct addrinfo* ai = create_addrinfo(ip_str, port);
                if(!http_client.PutRequest("http://www.baidu.com/img/baidu_jgylogo3.gif", ai, NULL, NULL, 
                        BatchConfig::DEFAULT_RES_PRIOR, BatchConfig::DEFAULT_BATCH_ID, ai))
                {
                    sleep(10);
                    LOG_ERROR("sleep ...\n");
                }
            }
    //http_client.PutRequest("http://www.baidu.com/");
    sleep(1000);
    return 0;
}
