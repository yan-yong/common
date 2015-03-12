/*
 * =====================================================================================
 *
 *       Filename:  TFetchResult.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/20/2009 11:42:00 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guo Shiwei (), guoshiwei@sohu-rd.com
 *        Company:  sohu
 *
 * =====================================================================================
 */

#include "TFetchResult.hpp"
#include "TFetchTask.hpp"

#include "utility/TUtility.hpp"
#include "spidernode_common.hpp"
#include "debug.h"

/*-----------------------------------------------------------------------------
 *  TFetchResult methods
 *-----------------------------------------------------------------------------*/
TFetchResult::TFetchResult(Resource *res, HttpFetcherResponse *resp)
    : res_(res), root_res_(TResExtension::rootRes(res_)),response(resp),
    error_group(FETCH_FAIL_GROUP_OK), error_code(0), release_rootres_(true)
{
    assert(res_ && root_res_);

    TResExtension::ResourceToUrl(root_res_, root_url_);
    if(!UriParse(root_url_.c_str(), root_url_.length(), root_uri_)
            || !HttpUriNormalize(root_uri_)){
        assert(false);
    }

    TResExtension *ext = TResExtension::getExt(res_);
    if(ext->redirectCount() > 0){
        TResExtension::ResourceToUrl(res_, redirect_url);

        if(!UriParse(redirect_url.c_str(), redirect_url.length(), redirect_uri_)
                || !HttpUriNormalize(redirect_uri_)){
            assert(false);
        }
    }

}

TFetchResult::~TFetchResult()
{
    if(response){
        delete response;
    }

    // We release res_ when:
    // 1. res_ is root resource and we can release it(release_rootres_ == true),
    // OR 2. res_ is not a root resource.
    // 
    // We CANNOT check the content of res_ itself because 
    // the res_ may already be released by another thread at this point.

    if(res_ == root_res_){
       if(release_rootres_){
           TResExtension::destroyResource(res_);
       }
    }
    else{
       if(release_rootres_){
           TResExtension::destroyResource(root_res_);
       }

       TResExtension::destroyResource(res_);
    }
}

HttpFetcherResponse * TFetchResult::getResponse()
{
    return response;
}

bool TFetchResult::isFetchSucceed()
{
    return error_group == FETCH_FAIL_GROUP_OK;
}

size_t TFetchResult::getRedirectCount() const
{

    return TResExtension::getExt(res_)->redirectCount();
}

void TFetchResult::getRedirectList(std::vector<std::string>& redirect_list){
int redirect_cnt = getRedirectCount();
Resource *tmp_res = res_;
Resource *tmp_root_res = root_res_;
for(int i=0; i<redirect_cnt && tmp_res!=tmp_root_res; i++){
	std::string tmp_url;
	TResExtension::ResourceToUrl(tmp_res, tmp_url);
	redirect_list.push_back(tmp_url);
	tmp_res = tmp_root_res;	
	tmp_root_res = TResExtension::rootRes(tmp_res);
}
std::string tmp_url;
TResExtension::ResourceToUrl(tmp_res, tmp_url);
redirect_list.push_back(tmp_url);
//·­×ª
std::reverse(redirect_list.begin(), redirect_list.end());
}

void TFetchResult::SetError(FETCH_FAIL_GROUP group, int error)
{
    error_group = group;
    error_code = error;
}

bool TFetchResult::IsRedirect() const
{
    TResExtension *ext = TResExtension::getExt(res_);
    return ext->redirectCount() > 0;
}

const std::string& TFetchResult::FinalUrl() const
{
    return redirect_url.empty() ? root_url_: redirect_url;
}

const URI& TFetchResult::FinalUri() const
{
    return redirect_url.empty() ? root_uri_: redirect_uri_;
}

FETCH_FAIL_GROUP TFetchResult::errorGroup() const
{
    return error_group;
}
int TFetchResult::errorCode() const
{
    return error_code;
}

const TFetchTask * TFetchResult::origTask() const
{
    return TResExtension::getOrigTask(res_);
}
