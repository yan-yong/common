/*
 * =====================================================================================
 *
 *       Filename:  TFetchResult.hpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/20/2009 11:39:07 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guo Shiwei (), guoshiwei@sohu-rd.com
 *        Company:  sohu
 *
 * =====================================================================================
 */


#ifndef  _SPIDERSERVICE_TFETCHRESULT_INC
#define  _SPIDERSERVICE_TFETCHRESULT_INC

#include "command/CmdTypes.hpp"
#include <string>

//head files in spider_kernel
#include <URI.hpp>

//head files in FetchKernel
#include "SchedulerTypes.hpp"
#include "HttpFetchProtocal.hpp"
#include <vector>

class TFetchTask;

/**
 * Constructed after a fetch is complete,
 * 
 * This will be the last holder of Allocated HttpFetcherResource,
 * will release the HttpFetcherResource when destructed.
 */

class TFetchResult
{
    private:
        /// The resource being fetched.
        Resource *res_;

        /// The root resource of `res_`.
        Resource *root_res_;

        /// Set if fetch succeed.
        HttpFetcherResponse* response;

        /// The root url. 
        std::string root_url_;

        /// Normolized format of root_url_; 
        URI root_uri_;

        /// The last redirected url 
        std::string redirect_url;
        URI redirect_uri_;

        FETCH_FAIL_GROUP error_group;
        int error_code; // Maybe: FETCH_ERROR, or HTTP status code, or HttpFetcher erro code.

        bool release_rootres_;
    public:

        /// @param res The current fetching resource.
        /// @param resp The response of `res`.
        TFetchResult(Resource *res,
                HttpFetcherResponse *resp);

        ~TFetchResult();

        void SetError(FETCH_FAIL_GROUP group, int error);

        HttpFetcherResponse * getResponse(); 

        /// \return the url of orignal request task
        const std::string &rootUrl() const {return root_url_;}
        const URI & rootURI() const {return root_uri_;}

        bool IsRedirect() const;

        /// return number of redirects;
        size_t getRedirectCount() const ;
    /// return all redirect url
    void getRedirectList(std::vector<std::string>& redirect_list);

        bool isFetchSucceed();
        FETCH_FAIL_GROUP errorGroup() const;
        int errorCode() const;

        const std::string& FinalUrl() const;
        const URI& FinalUri() const;

        const TFetchTask * origTask() const;

        /// Return the resource being fetched.
        Resource * res() {return res_;}

        /// Prevent TFetchResult release the resource when it is destructed.
        void releaseRootres(bool release){release_rootres_ = release;}
        bool releaseRootres(){return release_rootres_;}
};

class TFetchFail {
public:
    SSTask task_;
    FETCH_FAIL_GROUP error_group;
    FETCH_ERROR error_code;

    ~TFetchFail(){ task_.destroy(); }
};

typedef boost::shared_ptr<TFetchFail> TFetchFailPtr_t;

#endif   /* ----- #ifndef _SPIDERSERVICE_TFETCHRESULT_INC  ----- */
