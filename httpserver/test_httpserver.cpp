#include "httpserver.h"

int main()
{
    boost::shared_ptr<HttpServer> httpserver(new HttpServer());
    httpserver->initialize("0.0.0.0", "2768");
    httpserver->run();
    return 0;
}
