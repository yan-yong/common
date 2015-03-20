#include "HttpClient.hpp"

void HandleResult(boost::shared_ptr<HttpClient::FetchResult>)
{

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
