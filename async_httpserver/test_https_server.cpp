#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define CHECKRESULT( fun, ret)\
    if( (ret)<0) {fprintf(stderr, "line %d function "#fun" failed: %s\n",__LINE__,strerror(errno));}

typedef struct _ssl_server
{
    SSL_CTX*	ctx;
    char*		cafile;
    char*		keyfile;
    int			sock;//监听用的socket，默认绑定在443端口	
} SSLServer;

    int 
sock_select( int sock, int seconds, int useconds) 
{
    int ret;
    struct timeval tv;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET((unsigned)sock,&rfds);
    tv.tv_usec=useconds;
    tv.tv_sec=seconds;
    ret = select(sock+1,&rfds,NULL,NULL,&tv);
    FD_CLR((unsigned)sock,&rfds);
    return ret;
}

int main( int argc, char* argv[])
{
    if( argc < 3) {
        puts("Wrong usage");
        printf("%s $(Cert File) $(Key file)\n", argv[0]);
        return -1;
    }

    //创建SSL上下文
    SSLServer server;
    server.cafile = argv[1];
    server.keyfile = argv[2];
    SSL_load_error_strings( );
    SSLeay_add_ssl_algorithms( );
    server.ctx = SSL_CTX_new( SSLv23_server_method());
    if (SSL_CTX_use_certificate_file( server.ctx, server.cafile, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(3);
    }
    if (SSL_CTX_use_PrivateKey_file( server.ctx, server.keyfile, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(4);
    }                      
    if (SSL_CTX_check_private_key( server.ctx)<= 0){
        ERR_print_errors_fp(stderr);
        exit(4);
    }

    //创建监听socket
    int rc;
    server.sock = socket( PF_INET, SOCK_STREAM, 0);
    CHECKRESULT( socket, server.sock);
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = 0;
    addr.sin_family = AF_INET;
    addr.sin_port = htons( 4443);
    rc = bind( server.sock, (struct sockaddr*)&addr, sizeof(addr));
    CHECKRESULT( bind, rc);
    //rc = fcntl( server.sock, F_SETFL, O_NONBLOCK);
    //CHECKRESULT( fcntl, rc);
    rc = listen( server.sock, SOMAXCONN);
    CHECKRESULT( listen, rc);

    //主循环
    while( 1) {
        char buf[4096] = {0};
        int  newsock = -1;
        struct sockaddr from;
        long sz;
        socklen_t fromlen = sizeof(from);
        newsock = accept( server.sock, &from, &fromlen);
        if( newsock < 0 && EINTR == errno)
            continue;
        SSL* ssl = SSL_new( server.ctx);
        SSL_set_fd( ssl, newsock);
        SSL_accept( ssl);

        while( sock_select( newsock, 1, 0) > 0) 
        {
            sz = SSL_read( ssl, buf, sizeof(buf));
            if( sz <= 0)
                break;
            puts( buf);
            if(strstr(buf, "\r\n\r\n"))
                break;
        }
        strcpy(buf, "HTTP/1.1 200 OK\r\n"
                "Server:TESTSSL\r\n"
                "Content-Type:text/plain\r\n"
                "Connection:close\r\n"
                "Accept-Ranges: bytes\r\n"
                "Content-Length:14\r\n\r\n"
                "Hello OPENSSL!");
        SSL_write( ssl, buf, strlen(buf));                                                     
        SSL_free( ssl);

    }
    return 0;

}
