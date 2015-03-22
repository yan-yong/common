/**
 *   @author yanyong 
 */
#include "Fetcher.hpp"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/param.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <pthread.h>
#if ENABLE_SSL
# include <string.h>
# include <openssl/ssl.h>
#endif

#include <list.h>

#define IOBUFSIZE			(64 * 1024)
#define SECS_PER_MINUTE		60

#if ENABLE_SSL
# define SCHEME_USE_SSL(scheme)		((scheme) & 1)
# define SSL_ERROR_TO_ERRNO(error)	(256 + (error))
#endif

#define DEFAULT_QUEUE_MAX 500000

const struct timeval TIMEOUT_MS = {0, 1000};
#define SCHEME_USE_SSL(scheme)      ((scheme) & 1)

/**
 * State of connection to the Server
 * The sequence is:
 * CS_CLOSED -> CS_CONNECTING -> CS_READING 
 */
enum
{
    CS_CLOSED = 0,
    CS_CONNECTING,
    CS_READING,
#if ENABLE_SSL
    CS_CONNECTING_WANT_READ,
    CS_CONNECTING_WANT_WRITE,
    CS_WRITING_WANT_READ,
    CS_WRITING_WANT_WRITE,
    CS_READING_WANT_READ,
    CS_READING_WANT_WRITE,
#endif
    CS_FINISH,
    _CS_NTYPES
};

struct __connection
{
    /*all connection we are visiting are arranged in a list*/
    struct list_head list;
    /* The connection's state. */
    int state : 6;
    /* I hope enough. */
    unsigned int error : 10;
    int scheme;
    int sockfd;
    time_t resp_time;
    /* The current response message. */
    IFetchMessage *message;
    int socket_family;
    int socket_type;
    int protocol;
    FetchAddress address;
    void *user_data;
#if ENABLE_SSL
    SSL *ssl;
#endif
};

static void __address_string(const struct sockaddr* addr, 
    char* addrstr, size_t addrstr_length) 
{
    if (!addr) 
    {
        strncpy(addrstr, "0.0.0.0", addrstr_length);
        return;
    }    
    const char *ret = NULL;
    switch (addr->sa_family)
    {    
        case AF_INET:
            ret = inet_ntop(addr->sa_family, &((struct sockaddr_in *)addr)->sin_addr, addrstr, addrstr_length);
            break;
        case AF_INET6:
            ret = inet_ntop(addr->sa_family, &((struct sockaddr_in6 *)addr)->sin6_addr, addrstr, addrstr_length);
            break;
        default:
            break;
    }    
    if (!ret)
        strncpy(addrstr, "0.0.0.0", addrstr_length);
}

/*
 *  \return : maximum time of timeout milliseconds for epoll wait 
 */
static int GetEpollTimeOut(const struct timeval *epoll_timeout) {
    const long MILLION = 1000000;
    unsigned int timeout = 0;
    if (epoll_timeout) {
	timeout = epoll_timeout->tv_sec * MILLION + epoll_timeout->tv_usec;
	return timeout/1000;
    } else {
	return 0;
    }

    return timeout/1000;
}

static inline int GetConnTimeOut (const struct timeval *conn_timeout) {
    return conn_timeout->tv_sec ? conn_timeout->tv_sec : SECS_PER_MINUTE;
}

static int __should_close_message(Connection *conn)
{
    int need_close = 0;
    need_close = conn->state == CS_READING;
#if ENABLE_SSL
    need_close = need_close
	|| conn->state == CS_READING_WANT_READ
	|| conn->state == CS_READING_WANT_WRITE;
#endif
    return need_close;
}

static inline int __get_state_events(int state)
{
    switch (state)
    {
	case CS_CONNECTING:
	case CS_FINISH:
	    return EPOLLIN | EPOLLOUT | EPOLLET;
#if ENABLE_SSL
	case CS_CONNECTING_WANT_WRITE:
	case CS_WRITING_WANT_WRITE:
	case CS_READING_WANT_WRITE:
	    return EPOLLOUT | EPOLLET;
	case CS_CONNECTING_WANT_READ:
	case CS_WRITING_WANT_READ:
	case CS_READING_WANT_READ:
#endif
	case CS_READING:
	    return EPOLLIN | EPOLLET;
    }

    assert(!"Never here");
    return -1;
}

static int __add__event(Connection *conn, int epfd)
{
    struct epoll_event event;
    event.events = __get_state_events(conn->state);
    event.data.ptr = conn;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, conn->sockfd, &event);
}

static inline int __set_fd_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL);

    if (flags >= 0)
	flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    return flags;
}

static int __alloc_events_space(struct epoll_event **events,
	int nevents, int nconns)
{
    struct epoll_event *newbase;
    size_t size;

    if (nconns >= nevents) {
	nevents = 2 * nconns + 1;
	nevents = MAX(nevents, nconns + 1);
	size = nevents * sizeof (struct epoll_event);
	if ((newbase = (struct epoll_event *)realloc(*events, size)))
	    *events = newbase;
	else
	    nevents = -1;
    }

    return nevents;
}

Fetcher::Fetcher (IMessageEvents *message_events, IFetcherEvents *fetcher_events) : 
	epoll_events_(NULL), 
	nconns_(0), 
	max_events_(0),
	rx_speed_max_(0),
	total_rx_bytes_(0),
	total_tx_bytes_(0),
	last_total_rx_bytes_(0)
{
    INIT_LIST_HEAD(&conn_list_);
    INIT_LIST_HEAD(&new_conn_list_);
    message_events_ = message_events;
    fetch_events_ = fetcher_events;
    cur_time_ = time(NULL);
    gettimeofday(&last_rx_stat_time_, NULL);
    epfd_ = epoll_create(1); 
    SSL_library_init();
    ssl_ctx_ = SSL_CTX_new(SSLv3_client_method());
    ssl_ctx_->references = 100000;
    conn_count_ = (unsigned int*)calloc(_CS_NTYPES , sizeof(unsigned int));
    assert(ssl_ctx_);
}

Fetcher::~Fetcher ()
{
    Exit();
    free(conn_count_); 
}

void Fetcher::Exit()
{
    if (epoll_events_) {
	free(epoll_events_);
    }

    //INIT_LIST_HEAD(&__params->succ_list);
    struct list_head *pos, *tmp;
    Connection* conn;
    list_for_each_safe(pos, tmp, &conn_list_)
    {
	conn = list_entry(pos, Connection, list);
	list_del(pos);
	INIT_LIST_HEAD(&conn->list);
	if (__should_close_message(conn))
	{
	    message_events_->FreeFetchMessage(conn->message);
	    conn->message = NULL;
	}

#if ENABLE_SSL
	if (SCHEME_USE_SSL(conn->scheme) && conn->state != CS_CONNECTING) {
	    SSL_free(conn->ssl);
	    conn->ssl = NULL;
	}
#endif
	close(conn->sockfd);
	SetConnState(conn, CS_CLOSED);
    }

    if (epfd_ > 0) {
	close(epfd_);
    }
}

int Fetcher::GetConnCount(size_t *connecting, size_t *established, size_t * closed)
{
    if(connecting)
	*connecting = conn_count_[CS_CONNECTING];

    if(established){
	*established = conn_count_[CS_READING]
#if ENABLE_SSL
	    + conn_count_[CS_CONNECTING_WANT_READ]
	    + conn_count_[CS_CONNECTING_WANT_WRITE]
	    + conn_count_[CS_WRITING_WANT_READ]
	    + conn_count_[CS_WRITING_WANT_WRITE]
	    + conn_count_[CS_READING_WANT_READ]
	    + conn_count_[CS_READING_WANT_WRITE]
#endif
	;
    }

    if(closed)
	*closed = conn_count_[CS_CLOSED];
    return 0;
}

#if ENABLE_SSL
int Fetcher::SSLConnect(Connection *conn)
{
    assert(conn->ssl);
    int ret = SSL_connect(conn->ssl);
    int error;

    if (ret > 0)
    {
	if (SendRequest(conn) < 0)
	    return -1;
    }
    else
    {
	if ((error = SSL_get_error(conn->ssl, ret)) == SSL_ERROR_WANT_READ)
	    SetConnState(conn, CS_CONNECTING_WANT_READ);
	else if (error == SSL_ERROR_WANT_WRITE)
	    SetConnState(conn, CS_CONNECTING_WANT_WRITE);
	else
	{
	    if (error != SSL_ERROR_SYSCALL || errno == 0)
		errno = SSL_ERROR_TO_ERRNO(error);
	    return -1;
	}

	conn->resp_time = cur_time_;
    }

    return 0;
}

int Fetcher::SSLNew(Connection *conn)
{
    assert(conn);
    BIO *bio;
    if ((conn->ssl = SSL_new(ssl_ctx_)))
    {
	if ((bio = BIO_new_socket(conn->sockfd, BIO_NOCLOSE)))
	{
	    SSL_set_bio(conn->ssl, bio, bio);
	    return 0;
	}
    }

    errno = SSL_ERROR_TO_ERRNO(SSL_ERROR_SYSCALL);
    return -1;
}

int Fetcher::SSLInitialize(Connection *conn)
{
    assert(conn);
    if (SSLNew(conn) >= 0)
    {
	if (SSLConnect(conn) >= 0)
	    return 0;

	SSL_free(conn->ssl);
	conn->ssl = NULL;
    }

    return -1;
}

int Fetcher::SSLRead(Connection *conn, char *buf, int count)
{
    int ret = SSL_read(conn->ssl, buf, count);
    int error;

    if (ret >= 0)
	return ret;

    if ((error = SSL_get_error(conn->ssl, ret)) == SSL_ERROR_WANT_READ)
	SetConnState(conn, CS_READING_WANT_READ);
    else if (error == SSL_ERROR_WANT_WRITE)
	SetConnState(conn, CS_READING_WANT_WRITE);
    else
    {
	if (error != SSL_ERROR_SYSCALL || errno == 0)
	    errno = SSL_ERROR_TO_ERRNO(error);
	return -1;
    }

    errno = EAGAIN;
    return -1;
}

int Fetcher::SSLWritev(Connection *conn, const struct iovec *vector, int count)
{
    size_t size = 0;
    int i, ret;
    int error;
    void *buf;
    char *p;

    for (i = 0; i < count; i++)
	size += vector[i].iov_len;

    if (size == 0)
	return 0;

    if ((buf = malloc(size)))
    {
	p = (char *)buf;
	for (i = 0; i < count; i++)
	{
	    memcpy(p, vector[i].iov_base, vector[i].iov_len);
	    p += vector[i].iov_len;
	}

	assert(conn->ssl);
	ret = SSL_write(conn->ssl, buf, size);
	free(buf);
	if (ret > 0)
	    return 0;

	if ((error = SSL_get_error(conn->ssl, ret)) == SSL_ERROR_WANT_READ)
	    SetConnState(conn, CS_WRITING_WANT_READ);
	else if (error == SSL_ERROR_WANT_WRITE)
	    SetConnState(conn, CS_WRITING_WANT_WRITE);
	else
	{
	    if (error != SSL_ERROR_SYSCALL || errno == 0)
		errno = SSL_ERROR_TO_ERRNO(error);
	    return -1;
	}

	return 1;
    }

    return -1;
}

//static int __ssl_is_conn_alive(Connection *conn)
//{
//    int ret = SSL_read(conn->ssl, &ret, 1);
//    int error;
//
//    if (ret < 0)
//    {
//	error = SSL_get_error(conn->ssl, ret);
//	if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE)
//	    return 1;
//    }
//
//    return 0;
//}
//

#endif

int Fetcher::SendFetchRequest(const struct RequestData *request, Connection *conn)
{
    int new_state = CS_CLOSED;
    int n;

#if ENABLE_SSL
    if (SCHEME_USE_SSL(conn->scheme))
    {
	if ((n = SSLWritev(conn, request->vector, request->count)) == 0)
	    new_state = CS_READING_WANT_READ;
	else if (n < 0)
	    return -1;
    }
    else
#endif
    {
	if ((n = writev(conn->sockfd, request->vector, request->count)) >= 0){
	    new_state = CS_READING;
	}
	else
	    return -1;
    }

    if (new_state != CS_CLOSED)
    {
	conn->message = message_events_->CreateFetchResponse(conn->address, conn->user_data);
	if (!conn->message) {
	    return -1;
	}
	SetConnState(conn, new_state);
    }

    conn->resp_time = cur_time_;
    return 0;
}

int Fetcher::SendRequest(Connection *conn)
{
    struct RequestData *request = message_events_->CreateRequestData(conn->user_data);
    int i;

    if (request)
    {
	for(i=0; i <request->count; ++i) {
	    total_tx_bytes_ += request->vector[i].iov_len;
	}
	int ret = SendFetchRequest(request, conn);
	message_events_->FreeRequestData(request);
	return ret;
    } else {
	return -1;
    }
}

/**
 * @return 0 connect succeed.
 * 	   -1 socket failed
 */
int Fetcher::ConnectToServer(Connection *conn)
{
    conn->sockfd = socket(conn->socket_family, conn->socket_type, conn->protocol);
    if (conn->sockfd < 0) {
	return -1;
    }

    if (conn->address.local_addr != NULL) {
	if (bind(conn->sockfd, conn->address.local_addr, conn->address.local_addrlen) == -1) {
	    assert(false);
	    return -1;
	}
    }

    if (__set_fd_nonblock(conn->sockfd) >= 0)
    {
	if (connect(conn->sockfd, conn->address.remote_addr, conn->address.remote_addrlen) < 0)
	{
	    if (errno == EINPROGRESS)
	    {
		SetConnState(conn, CS_CONNECTING);
		conn->resp_time = cur_time_;
		return 0;
	    }
	}
#if ENABLE_SSL
	else if (SCHEME_USE_SSL(conn->scheme))
	{
	    if (SSLInitialize(conn) >= 0)
		return 0;
	}
#endif
	else if (SendRequest(conn) >= 0) {
	    return 0;
	}
    }

    close(conn->sockfd);
    return -1;
}

void Fetcher::UpdateTime(time_t cur_time)
{
    cur_time_ = cur_time;
}

int Fetcher::NewConnection(Connection *conn)
{
    int ret;

    ret = ConnectToServer(conn);
    if (ret == 0)
    {
	if (__add__event(conn, epfd_) < 0) {
	    ret = -1;
	}
    }

    if (ret == 0) {
	return 0;
    } else {
	return -1;
    }

}

int Fetcher::AddConn(int *n, Connection *conn, struct list_head *conn_list)
{
    if (conn->state == CS_FINISH) {
	if (__add__event(conn, epfd_) < 0) {
	    assert(false);
	}
	list_add_tail(&conn->list, conn_list);
	(*n)++;
	return 0;
    }
    if (NewConnection(conn) == 0) {
	list_add_tail(&conn->list, conn_list);
	(*n)++;
	return 0;
    } else {
	return -1;
    }
}

void Fetcher::AddConnList(int *n, struct list_head *conn_list)
{
    struct list_head *pos, *prev;
    Connection *conn;
    list_for_each_safe(pos, prev, conn_list)
    {
	conn = list_entry(pos, Connection, list);
	list_del(pos);
	INIT_LIST_HEAD(&conn->list);
	if (AddConn(n, conn, &conn_list_) != 0) {
	    SetConnError(conn, errno);
	}
    }
}

/**
 * Read data from conn
 * \return -1 We have error
 * \return 0 EAGAIN, we should keep on reading, 
 * \return 1 One fetch finished, 
 */
int Fetcher::ReadData(Connection *conn, struct epoll_event *event)
{
    int alive;
    int n = ReadFromConn(conn, &alive);

    if (n > 0) {
	conn->resp_time = cur_time_;
	return 0;
    } else if (n < 0) {
	return -1;
    }

    alive = alive && conn->message->IsKeepAlive();
    
    SetConnState(conn, CS_FINISH);
    epoll_ctl(epfd_, EPOLL_CTL_DEL, conn->sockfd, event);
    //ssl多线程需加锁，故ssl连接不支持长连接
    if (SCHEME_USE_SSL(conn->scheme)) {
	CloseConnection(conn);
    }
    
    bool inst_fetch = fetch_events_->FinishFetch(conn, conn->user_data, conn->message);

    //support keep alive
    if (alive) {
	if (inst_fetch) {
	    if (SendRequest(conn) >= 0) {
		epoll_ctl(epfd_, EPOLL_CTL_ADD, conn->sockfd, event);
		return 0;
	    } else {
		return -1;
	    }
	} else {
	    nconns_--;
	}
	return 1;
    }

    //CloseConnection放到外围close 
    return 1;
}

/**
 * Read data from conn
 * \return -1 We have error
 * \return 0 read finished, 
 * \return 1 EAGAIN, 
 */
int Fetcher::ReadFromConn(Connection *conn, int *alive)
{
    static char buf[IOBUFSIZE];
    int n, ret;

    if(rx_speed_max_ > 0 && total_rx_bytes_ - last_total_rx_bytes_ >= rx_speed_max_){
	struct timeval now;
	gettimeofday(&now, NULL);
	long diffusec = 1000000L - ((now.tv_sec - last_rx_stat_time_.tv_sec) * 1000000L + 
		(now.tv_usec - last_rx_stat_time_.tv_usec));
	if(diffusec > 0){
	    struct timeval sleeptime;
	    if(diffusec == 1000000L){
		sleeptime.tv_sec = 1;
		sleeptime.tv_usec = 0;
	    }
	    else
	    {
		sleeptime.tv_sec = 0;
		sleeptime.tv_usec = diffusec;
	    }
	    select(1, NULL, NULL, NULL, &sleeptime);
	}
	gettimeofday(&last_rx_stat_time_, NULL);
	last_total_rx_bytes_ = total_rx_bytes_;
    }

    do
    {
#if ENABLE_SSL
	if (SCHEME_USE_SSL(conn->scheme))
	    n = SSLRead(conn, buf, IOBUFSIZE);
	else
#endif
	    n = read(conn->sockfd, buf, IOBUFSIZE);

	if (n >= 0) {
	    total_rx_bytes_ += n;
	}
	else if (errno == EAGAIN)
	    return 1;
	else
	    return -1;

	if ((ret = conn->message->Append(buf, n)) < 0) {
	    return -1;
	}
    } while (n && ret);

    *alive = !!n;
    return 0;
}

/*
 * \return: 0 success
 *          -1 error
 */
int Fetcher::CompleteConnection(Connection *conn)
{
    socklen_t n = sizeof (int);
    int error;

    if (getsockopt(conn->sockfd, SOL_SOCKET, SO_ERROR, &error, &n) >= 0)
    {
	if (error == 0)
	{
#if ENABLE_SSL
	    if (SCHEME_USE_SSL(conn->scheme))
	    {
		if (SSLInitialize(conn) >= 0)
		    return 0;
	    }
	    else
#endif
	    {
		if (SendRequest(conn) >= 0)
		{
		    return 0;
		}
	    }
	}
	else
	    errno = error;
    }

    return -1;
}

void Fetcher::SetConnState(Connection *conn, int state)
{
    conn_count_[conn->state]--;
    conn_count_[state]++;
    conn->state = state;
}


void Fetcher::RemoveErrorConn(Connection *conn, int error)
{
#if ENABLE_SSL
    if (SCHEME_USE_SSL(conn->scheme) && conn->state != CS_CONNECTING) {
	SSL_free(conn->ssl);
	conn->ssl = NULL;
    }
#endif

    if (__should_close_message(conn)) {
	message_events_->FreeFetchMessage(conn->message);
	conn->message = NULL;
    }
    close(conn->sockfd);
    SetConnState(conn, CS_CLOSED);
    SetConnError(conn, error);
}

int Fetcher::GetTrafficBytes(uint64_t *rx_bytes, uint64_t *tx_bytes) {
    *rx_bytes = total_rx_bytes_;
    *tx_bytes = total_tx_bytes_;
     return 0;
}

Connection* Fetcher::CreateConnection(
		int scheme,
		int socket_family,
		int socket_type,
		int protocol,
		const FetchAddress& address
		)
{
    if (address.remote_addr == NULL || address.remote_addrlen <= 0 ) { 
	assert(false);
	return NULL;
    }

    Connection *conn = (Connection*)malloc(sizeof(Connection));
    if (conn) {
	INIT_LIST_HEAD(&conn->list);
	conn->state = CS_CLOSED; 
	conn->error = 0;
	conn->scheme = scheme;
	conn->sockfd = -1;
	conn->message = NULL;
	conn->socket_family = socket_family;
	conn->socket_type = socket_type;
	conn->protocol = protocol;
	
	assert(address.remote_addrlen == sizeof(struct sockaddr));
	conn->address.remote_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
	memcpy(conn->address.remote_addr, address.remote_addr, sizeof(struct sockaddr));
	conn->address.remote_addrlen = address.remote_addrlen;
	conn->address.local_addrlen = address.local_addrlen;
	if (conn->address.local_addrlen > 0) {
	    conn->address.local_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
	    memcpy(conn->address.local_addr, address.local_addr, sizeof(struct sockaddr));
	} else {
	    conn->address.local_addr = NULL;
	}
	conn->ssl = NULL;
	conn->user_data = NULL;
    }
    return conn;
}
    
void Fetcher::FreeConnection (Connection *conn) {
    if (conn) {
	assert(conn->state == CS_CLOSED);
	assert(list_empty(&conn->list));
	conn->user_data = NULL;
	free(conn->address.remote_addr);
	if (conn->address.local_addr) {
	    free(conn->address.local_addr);
	}
	free(conn);
    }
}

int ThreadingFetcher::GetSockAddr(Connection* conn, struct sockaddr* addr)
{
    if (!conn) {
	return -1;
    }
    *addr = *conn->address.remote_addr;
    return 0;
}

void ThreadingFetcher::ConnectionToString(Connection * conn, char* str, size_t str_len)
{
    __address_string(conn->address.local_addr, str, str_len/2);
    strcat(str, " ==> ");
    size_t cur_len = strnlen(str, str_len);
    __address_string(conn->address.remote_addr, str + cur_len, str_len - cur_len);
}

void Fetcher::CloseConnection (Connection *conn) {
    assert(list_empty(&conn->list));
    if (conn->state != CS_CLOSED) {
#if ENABLE_SSL
	if (SCHEME_USE_SSL(conn->scheme)) {
	    if (conn->ssl) {
		SSL_free(conn->ssl);
		conn->ssl = NULL;
	    }
	}
#endif
	close(conn->sockfd);
	conn->sockfd = -1;
	SetConnState(conn, CS_CLOSED);
    }
}

void Fetcher::StartRequest (Connection *conn, void *context) {
    assert(conn->state == CS_CLOSED || conn->state == CS_FINISH);
    assert(list_empty(&conn->list));
    conn->user_data = context;
    list_add_tail(&conn->list, &new_conn_list_);
}

void Fetcher::SetConnError(Connection *conn, int error)
{
    list_del(&conn->list);
    INIT_LIST_HEAD(&conn->list);
    conn->error = error;
    nconns_--;
    fetch_events_->FetchError(conn, conn->user_data, error);
}

/*
 *  \return 0: connection event change or keep reading
 *  \return 1: fetch finish or fetch error
 */
int Fetcher::CheckEvent(struct epoll_event *event, struct list_head *conn_list) {
    Connection *conn = (Connection *)event->data.ptr;
    int old_state = conn->state;
    int ret;
    
    switch (conn->state)
    {
	case CS_CONNECTING:
	case CS_FINISH:
	    ret = CompleteConnection(conn);
	    break;
#if ENABLE_SSL
	case CS_CONNECTING_WANT_READ:
	case CS_CONNECTING_WANT_WRITE:
	    ret = SSLConnect(conn);
	    break;
	case CS_WRITING_WANT_READ:
	case CS_WRITING_WANT_WRITE:
	    ret = SendRequest(conn);
	    break;
	case CS_READING_WANT_READ:
	case CS_READING_WANT_WRITE:
#endif
	case CS_READING:
	    ret = ReadData(conn, event);
	    if (ret <= 0)
		break;
	    return ret;
	    
	default:
	    assert(!"Never here");
    }
    
    if (ret >= 0)
    {
	int new_events = __get_state_events(conn->state);
	int old_events = __get_state_events(old_state);

	if (new_events != old_events)
	{
	    event->events = new_events;
	    ret = epoll_ctl(epfd_, EPOLL_CTL_MOD, conn->sockfd, event);
	}

	if (ret >= 0)
	{
	    return 0;
	}
    }

    // ret < 0 
    RemoveErrorConn(conn, errno);
    return 1;
}

void Fetcher::Poll (const Params &params, const struct timeval *timeout) {

    //connnection list that we made a connection to.
    LIST_HEAD(tmp_conn_list);
    list_splice_init(&new_conn_list_, tmp_conn_list.prev);

    rx_speed_max_ = params.rx_speed_max;
    int newconns = 0;

    AddConnList(&newconns, &tmp_conn_list);
    int n = 0;

    UpdateTime(time(NULL));
    nconns_ += newconns;

    unsigned int epoll_timeout = GetEpollTimeOut(timeout);
    if ((n = __alloc_events_space(&epoll_events_, max_events_, nconns_)) >= 0) {
	max_events_ = n;
	n = epoll_wait(epfd_, epoll_events_, nconns_ + 1, epoll_timeout);
    }

    UpdateTime(time(NULL));

    if (n >= 0) {
	for (int i = 0; i < n; i++) {
	    Connection* conn = (Connection *)(epoll_events_ + i)->data.ptr;
	    list_del(&conn->list);
	    INIT_LIST_HEAD(&conn->list);
	    if (CheckEvent(epoll_events_ + i, &conn_list_) == 0) {
		list_add_tail(&conn->list, &conn_list_);
	    }
	}

	Connection *conn = NULL;
	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, &conn_list_) {
	    conn = list_entry(pos, Connection, list);
	    if (cur_time_ >= conn->resp_time + GetConnTimeOut(&params.conn_timeout)) {
		RemoveErrorConn(conn, ETIMEDOUT);
	    } else {
		break;
	    }
	}
    } else if (errno != EINTR) {
	return;
    }
}

ThreadingFetcher::ThreadingFetcher(IMessageEvents *message_events):
	request_queue_max_(DEFAULT_QUEUE_MAX),
	result_queue_max_(DEFAULT_QUEUE_MAX),
	stop_(true), param_changed_(false)
{
    fetcher_.reset(new Fetcher(message_events, this));
    pthread_mutex_init(&param_mutex_, NULL);
    pthread_mutex_init(&request_queue_mutex_, NULL);
    pthread_mutex_init(&result_queue_mutex_, NULL);
    pthread_cond_init(&result_queue_not_empty_cond_, NULL);
    pthread_cond_init(&result_queue_not_full_cond_, NULL);
}

ThreadingFetcher::~ThreadingFetcher() {
    pthread_mutex_destroy(&param_mutex_);
    pthread_mutex_destroy(&request_queue_mutex_);
    pthread_mutex_destroy(&result_queue_mutex_);
    pthread_cond_destroy(&result_queue_not_empty_cond_);
    pthread_cond_destroy(&result_queue_not_full_cond_);
}

int ThreadingFetcher::Begin(const Fetcher::Params &params) {
    UpdateParams(params);
    int ret;
    if (stop_) {
	stop_ = false;
	ret = pthread_create(&tid_, NULL, RunThread, this);
	if (ret == 0) {
	    return 0;
	} else {
	    stop_ = true;
	    errno = ret;
	    return -1;
	}
    } else {
	return 1;
    }
}

void ThreadingFetcher::End() {
    void *ret;

    if (!stop_) {
	stop_ = true;
	pthread_join(tid_, &ret);
    }
    stop_ = false;
}

void ThreadingFetcher::UpdateParams(const Fetcher::Params& params) {
    pthread_mutex_lock(&param_mutex_);
    memcpy(&params_, &params, sizeof(Fetcher::Params));
    param_changed_ = true;
    pthread_mutex_unlock(&param_mutex_);
}

void* ThreadingFetcher::RunThread(void *context) {
    ThreadingFetcher* fetcher = (ThreadingFetcher *)context;
    fetcher->Run();
    return 0;
}

unsigned ThreadingFetcher::AvailableQuota()
{
    return request_queue_.size() >= request_queue_max_ ? 
        0 : request_queue_max_ - request_queue_.size();
}

void ThreadingFetcher::Run() {
	Fetcher::Params params;
    while(!stop_) 
    {
        if(param_changed_)
        {
            pthread_mutex_lock(&param_mutex_);
            memcpy(&params, &params_, sizeof(Fetcher::Params));
            param_changed_ = false;
            pthread_mutex_unlock(&param_mutex_);
        }
        //get request
        if(!request_queue_.empty())
        {
            pthread_mutex_lock(&request_queue_mutex_);
            while (!request_queue_.empty()) 
            {
                if(params.max_connecting_cnt)
                {
                    size_t connecting = 0, established = 0, closed = 0;
                    fetcher_->GetConnCount(&connecting, &established, &closed);
                    if(connecting + established > params.max_connecting_cnt)
                        break;
                }
                RawFetcherRequest request = request_queue_.front();
                request_queue_.pop();
                fetcher_->StartRequest(request.conn, request.context);
            }
            pthread_mutex_unlock(&request_queue_mutex_);
        }
	    fetcher_->Poll(params, &TIMEOUT_MS);
    }
}

Connection* ThreadingFetcher::CreateConnection(
		int scheme,
		int socket_family,
		int socket_type,
		int protocol,
		const FetchAddress& address
		)
{
    return Fetcher::CreateConnection(scheme, socket_family, socket_type, protocol, address);
}

void ThreadingFetcher::FreeConnection(Connection *conn) 
{
    Fetcher::FreeConnection(conn);
}

void ThreadingFetcher::CloseConnection (Connection *conn) {
    fetcher_->CloseConnection(conn);
}

int ThreadingFetcher::GetTrafficBytes(uint64_t *rx_bytes, uint64_t *tx_bytes) {
    return fetcher_->GetTrafficBytes(rx_bytes, tx_bytes);
}

int ThreadingFetcher::GetConnCount(size_t *connecting, size_t *established, size_t * closed) {
    return fetcher_->GetConnCount(connecting, established, closed);
}

int ThreadingFetcher::PutRequest(const RawFetcherRequest& request) {
    assert(request.conn);
    assert(list_empty(&request.conn->list));
    pthread_mutex_lock(&request_queue_mutex_);
    if (request_queue_.size() > request_queue_max_) {
	pthread_mutex_unlock(&request_queue_mutex_);
	return -1;
    }
    request_queue_.push(request);
    pthread_mutex_unlock(&request_queue_mutex_);
    return 0;
}

void ThreadingFetcher::PutResult(const RawFetcherResult& result) {
    pthread_mutex_lock(&result_queue_mutex_);
    result_queue_.push(result);
    if (result_queue_.size() == 1) {
        pthread_cond_signal(&result_queue_not_empty_cond_);
    } else if (result_queue_.size() == result_queue_max_) {
	pthread_cond_wait(&result_queue_not_full_cond_, &result_queue_mutex_);
    }
    pthread_mutex_unlock(&result_queue_mutex_);
}

int ThreadingFetcher::GetResult(struct RawFetcherResult *result, const struct timeval *timeout) {
    if(!timeout){
	pthread_mutex_lock(&result_queue_mutex_);

	while (result_queue_.empty())
	    pthread_cond_wait(&result_queue_not_empty_cond_, &result_queue_mutex_);

	*result = result_queue_.front();
	result_queue_.pop();
	if (result_queue_.size() == result_queue_max_ - 1) {
	    pthread_cond_signal(&result_queue_not_full_cond_);
	}
	pthread_mutex_unlock(&result_queue_mutex_);
	return 0;
    }

    struct timespec abstime;
    struct timeval now;
    int ret = 0;

    pthread_mutex_lock(&result_queue_mutex_);

    gettimeofday(&now, NULL);
    abstime.tv_sec = now.tv_sec + timeout->tv_sec;
    abstime.tv_nsec = 1000 * (now.tv_usec + timeout->tv_usec);

    // abstime normalize
    const long BILLION = 1000000000;
    if (abstime.tv_nsec >= BILLION)
    {
	abstime.tv_sec += abstime.tv_nsec / BILLION;
	abstime.tv_nsec %= BILLION;
    }

    while (result_queue_.empty() && ret != ETIMEDOUT) {
	ret = pthread_cond_timedwait(&result_queue_not_empty_cond_, &result_queue_mutex_, &abstime);
    }

    if (ret != ETIMEDOUT)
    {
	*result = result_queue_.front();
	result_queue_.pop();
	pthread_cond_signal(&result_queue_not_full_cond_);
	ret = 0;
    } else {
	ret = 1;
    }

    pthread_mutex_unlock(&result_queue_mutex_);
    return ret;
}

bool ThreadingFetcher::FinishFetch(Connection *conn, void *request_context, IFetchMessage *message) {
    RawFetcherResult result;
    result.conn = conn;
    result.message = message;
    result.err_num = 0;
    result.context = request_context;
    PutResult(result);
    return false;
}

void ThreadingFetcher::FetchError(Connection *conn, void *request_context, int err_num) {
    RawFetcherResult result;
    result.conn = conn;
    result.message = NULL;
    result.err_num = err_num;
    result.context = request_context;
    PutResult(result);
}
