/**
 *  @author: yanyong 
 */

#ifndef  FETCHER_INC
#define  FETCHER_INC
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <netdb.h> 
#include <openssl/ssl.h>
#include <boost/shared_ptr.hpp> 
#include <queue> 
#include "list.h"

/**
 * Data to send to remote server
 */
struct RequestData {
    struct iovec *vector;
    int count;
};

/** 
 * Abstract interface for recving data of fetch result.
 * Called when new data recieved on the cresponding connection,
 */
class IFetchMessage {
    public:
	virtual int Append(const void * data, size_t length) = 0;
	virtual bool IsKeepAlive() const = 0;

	virtual ~IFetchMessage(){}
};

struct FetchAddress {
    struct sockaddr* remote_addr;
    size_t remote_addrlen;
    struct sockaddr* local_addr;
    size_t local_addrlen;
};

struct __connection;
typedef struct __connection Connection;

class IMessageEvents {
    public:
	/**
	 *  Create and Free RequestData
	 */
	virtual struct RequestData* CreateRequestData(void * request_context) = 0;
	virtual void FreeRequestData(struct RequestData * request_data) = 0;
	
	/**
	 * Create IFetchMessage instance.
	 */
	virtual IFetchMessage* CreateFetchResponse(const FetchAddress& address, void * request_context) = 0;
	virtual void FreeFetchMessage(IFetchMessage *fetch_message) = 0;
	virtual ~IMessageEvents(){}
};

class IFetcherEvents {
    public:
	/**
	 * Called when recv one IFetchMessage Finished.
	 *
	 * \return true to keep the connection.
	 * \return false to close the connection.
	 */
	virtual bool FinishFetch(Connection* conn, void *request_context, IFetchMessage *message) = 0;

	/**
	 * Called when recv one IFetchMessage when some error occur.
	 * Fetch Failed.
	 */
	virtual void FetchError(Connection* conn, void *request_context, int err_num) = 0;
	
	virtual ~IFetcherEvents(){}
};

struct RawFetcherRequest {
    Connection* conn;
    void* context;
};

struct RawFetcherResult {
    Connection* conn;
    IFetchMessage *message;
    int err_num;
    void* context;
};

class Fetcher {
    public:
	/**
	 * Params controlling the behiver of fetch
	 */
	struct Params 
	{
	    struct timeval conn_timeout;    // 在以下网络操作上的超时时间:
					    // connect, read
	    unsigned int rx_speed_max;      // 最大入流量，bytes/seconds
					    // 超过此流量时read会阻塞，强行降低入流量
        unsigned int max_connecting_cnt;// 最大并发连接数目
	};

    public:
	Fetcher(IMessageEvents *message_events, IFetcherEvents *fetcher_events);
	virtual ~Fetcher();

	/**
	 * 创建一个连接句柄，客户拿到连接句柄后可以通过StartRequest发起请求。
	 *
	 * @param request_context: 对Fetcher透明的指针，
	 *	    在CreateRequestData, CreateFetchResponse时为用户提供上下文
	 *
	 * StartRequest 之后，Fetcher会依次回调以下方法
	 *	- CreateRequestData:
	 *	    客户此时应返回RequestData对象，Fetcher负责将数据发送给远端服务器
	 *	- FreeRequestData:
	 *	    RequestData发送完毕，释放
	 *	- CreateFetchResponse:
	 *	    准备接收回应，客户应创建IFetchMessage对象接收数据
	 *	- while(IFetchMessage::Append() == 0)
	 *	      ;
	 *	    不断接收数据
	 *	- FinishFetch() 结束一轮抓取。
	 *	    客户可通过返回true或false指示Fetcher是否立即发起下一次请求
	 *	- IFetchMessage::IsKeepAlive(): 
	 *	    客户通过返回true或false指示是否支持长连接
	 *
	 *	- FreeFetchMessage: Fetcher释放对IFetchMessage的引用
	 *	    此后Fetcher不再持有IFetchMessage对象，
	 *	    客户可在此时处理并销毁IFetchMessage对象
	 *
	 * @NOTE:
	 *	- 一个句柄上同时只能发起一个请求，但对于一个(LocalAddress, RemoteAddress)来说，
	 *	    可以建立任意多个Connection.
	 *	- FinishFetch之后，不必立即调用FreeConnection释放Connection,
	 *	    可以在此句柄上再发起请求
	 *  - 何时FreeConnection由客户控制，但注意别发生资源泄漏。
	 */
	static Connection * CreateConnection(
		int scheme,
		int socket_family,
		int socket_type,
		int protocol,
		const FetchAddress& address
		);
	static void FreeConnection(Connection *conn);
	void CloseConnection (Connection *conn);

	/**
	 * 发起抓取请求，使Connection进入与远程服务器的PingPong循环。
	 * 但请注意，StartRequest必须在FinishFetch或CreateConnection之后
	 */
	void StartRequest(Connection *conn, void *context);

	/**
	 * 进行实际的网络IO驱动，阻塞最多timeout时长（如果timeout != 0)
	 */
	void Poll(const Fetcher::Params &params, const struct timeval *timeout);
	int GetTrafficBytes(uint64_t *rx_bytes, uint64_t *tx_bytes); 
	int GetConnCount(size_t *connecting, size_t *established, size_t * closed);
      
    private:
	int CheckEvent(struct epoll_event *event, struct list_head *conn_list);
	void AddConnList(int *n, struct list_head *conn_list);
	int AddConn(int *n, Connection *conn, struct list_head *conn_list);
	int NewConnection(Connection *conn);
	int ConnectToServer(Connection *conn);
#if ENABLE_SSL
	int SSLNew(Connection *conn);
	int SSLInitialize(Connection *conn);
	int SSLConnect(Connection *conn);
	int SSLRead(Connection *conn, char *buf, int count);
	int SSLWritev(Connection *conn, const struct iovec *vector, int count);
#endif
	int SendRequest(Connection *conn);
	int SendFetchRequest(const struct RequestData *request, Connection *conn);
	int ReadData(Connection *conn, struct epoll_event *event);
	int ReadFromConn(Connection *conn, int *alive);
	int CompleteConnection(Connection *conn);
	void SetConnState(Connection *conn, int state);

	void SetConnError(Connection *conn, int error);
	void RemoveErrorConn(Connection *conn, int error);
	void Exit();
	void UpdateTime(time_t cur_time);

    protected:
	IMessageEvents* message_events_;
	IFetcherEvents* fetch_events_;

    private:
	struct epoll_event* epoll_events_;
	int epfd_;
	int nconns_;
	int max_events_;
	unsigned int rx_speed_max_;
	SSL_CTX* ssl_ctx_;
	unsigned int* conn_count_;

	struct list_head conn_list_;    //connection which is fetching
	struct list_head new_conn_list_;
	time_t cur_time_;
	struct timeval last_rx_stat_time_;
	uint64_t total_rx_bytes_;
	uint64_t total_tx_bytes_; 
	uint64_t last_total_rx_bytes_;
};

class ThreadingFetcher : IFetcherEvents {
    public:
 	ThreadingFetcher(IMessageEvents *message_events);
	virtual ~ThreadingFetcher();

    /*** static method ***/
	static Connection * CreateConnection(
		int scheme,
		int socket_family,
		int socket_type,
		int protocol,
		const FetchAddress& address
		);
	static void FreeConnection(Connection *conn);
	static int GetSockAddr(Connection* conn, struct sockaddr* addr);
    static void ConnectionToString(Connection * conn, char* str, size_t str_len); 

	void CloseConnection (Connection *conn);
	int Begin(const Fetcher::Params& params);
	void End();
	void UpdateParams(const Fetcher::Params &params);
	int PutRequest(const RawFetcherRequest& request);
	int GetResult(struct RawFetcherResult *result, const struct timeval *timeout = NULL);
	void UpdateFetcherParam(const Fetcher::Params &fetch_param);
	int GetConnCount(size_t *connecting, size_t *established, size_t * closed);
	int GetTrafficBytes(uint64_t *rx_bytes, uint64_t *tx_bytes); 
    unsigned AvailableQuota();

    protected:
	virtual bool FinishFetch(Connection* conn, void *request_context, IFetchMessage *message);
	virtual void FetchError(Connection* conn, void *request_context, int err_num);

    private:
	void Run();
	static void* RunThread(void *context);
	void PutResult(const RawFetcherResult& result);

    protected:
	IFetcherEvents* threading_fetch_events_;

    private:
	Fetcher::Params params_;
	pthread_mutex_t param_mutex_;

	std::queue<RawFetcherRequest> request_queue_;
	pthread_mutex_t request_queue_mutex_;
	size_t request_queue_max_; 

	pthread_mutex_t result_queue_mutex_;
	pthread_cond_t result_queue_not_empty_cond_;
	pthread_cond_t result_queue_not_full_cond_;
	std::queue<RawFetcherResult> result_queue_;
	size_t result_queue_max_; 

	bool stop_;
	pthread_t tid_;

	boost::shared_ptr<Fetcher> fetcher_;
    bool param_changed_;
};

#endif   /* ----- #ifndef FETCHER_INC  ----- */
