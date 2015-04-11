#include "HttpClient.hpp"
#include "utility/net_utility.h"

void HandleResult(boost::shared_ptr<HttpClient::FetchResult> result)
{
    HttpFetcherResponse* resp = result->resp_;
    if(resp)
        fprintf(stderr, "Size: %zd\nContent: %s\n", resp->Body.size(), &(resp->Body[0]));
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

    struct addrinfo *ai = create_addrinfo("84.145.138.23", 80);
    //struct addrinfo *ai = create_addrinfo("202.108.250.249", 443);
    http_client.PutRequest("https://i.alipayobjects.com/i/ecmng/png/201501/4Jdkug9K2v.png",
        NULL, NULL, NULL, BatchConfig::DEFAULT_RES_PRIOR, BatchConfig::DEFAULT_BATCH_ID, ai);
    sleep(1000);
    return 0;
}
