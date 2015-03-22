#include "HttpClient.hpp"

void HandleResult(boost::shared_ptr<HttpClient::FetchResult> result)
{
    HttpFetcherResponse* resp = result->resp_;
    if(resp)
        printf("%zd\n", resp->Body.size());
}

int main()
{
    HttpClient http_client;
    http_client.Open();
    http_client.SetResultCallback(HandleResult);
    http_client.PutRequest("http://www.baidu.com/");
    http_client.PutRequest("http://www.baidu.com/");
    sleep(1000);
    return 0;
}
