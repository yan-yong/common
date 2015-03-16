#include "HttpClient.hpp"

int main()
{
    HttpClient::ResultCallback result_cb;
    HttpClient http_client;
    http_client.SetResultCallback(result_cb);
}
