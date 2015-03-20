#include "HttpClient.hpp"

void HandleResult(boost::shared_ptr<HttpClient::FetchResult> result)
{
    HttpFetcherResponse* resp = result->resp_;
    printf("%s\n", &(resp->Body[0]));
}

int main()
{
    HttpClient http_client;
    http_client.Open();
    http_client.SetResultCallback(HandleResult);
    http_client.PutRequest("http://www.baidu.com/");
    sleep(1000);
    return 0;
}
