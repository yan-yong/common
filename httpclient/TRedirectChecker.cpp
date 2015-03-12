/*
 * =====================================================================================
 *
 *       Filename:  TRedirectChecker.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/14/2009 02:52:08 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guo Shiwei (), guoshiwei@sohu-rd.com
 *        Company:  sohu
 *
 * =====================================================================================
 */

#include <pthread.h>
#include <stdio.h>

#include "TRedirectChecker.hpp"
#include "httpparser/URI.hpp"
#include "httpparser/olduri.h"
#include "httpparser/HttpMessage.hpp"
#include "httpparser/hlink.h"
#include "httpparser/script.h"
#include "TUtility.hpp"

#include "ScheduleKernelTypes.hpp"

static pthread_mutex_t sr_mutex = PTHREAD_MUTEX_INITIALIZER;
boost::shared_ptr < TRedirectChecker > TRedirectChecker::instance_;

boost::shared_ptr < TRedirectChecker > &TRedirectChecker::instance() 
{
    if (!instance_) 
    {
        throw std::runtime_error("TRedirectChecker instance not set");
    }
    return instance_;
}

void TRedirectChecker::instance(boost::shared_ptr < TRedirectChecker > &obj) 
{
    instance_ = obj;
}

class TRedirectChecker::Impl 
{
    public:
        Impl()
        {
        }

        ~Impl() 
        {
        }

        bool checkMetaRedirect(const std::string & from,
            const Response & resp, std::string & result) 
        {
            if (!isHtml(resp)) 
                return false;

            URI from_uri;
            if (!UriParse(from.c_str(), from.size(), from_uri)
                    || !HttpUriNormalize(from_uri)) 
            {
                return false;
            }

            if (metaRedirct(from_uri, &resp.Body[0], resp.Body.size(), result)) 
            {
                return true;
            }

            return false;
        }

#if 0
        bool checkScriptRedirect(const std::string & from,
                const Response & resp, std::string & result) 
        {
            if (!isHtml(resp)) 
                return false;
            return scriptRedirect(from, resp, result);
        }
#endif

    private:
#if 0
        bool scriptRedirect(const std::string & from,
            const Response & resp, std::string & result) 
        {
            //use local scriptredirect checker
            URI from_uri;
            if (!UriParse(from.c_str(), from.size(), from_uri)
                    || !HttpUriNormalize(from_uri)) {
                return false;
            }
            return findScriptRedirect(from_uri, &resp.Body[0], resp.Body.size(), result);
        }
#endif
        size_t maxTextSize() 
        {
            // TODO: get this from config: MaxTextSize at 'Spider/Redirect'
            return 200;
        }

        REDIRECT_TYPE getRedirectType(const char *r_type_str)
        {
            if (strcasecmp(r_type_str, "REDIRECT_TYPE_SCRIPT") == 0) {
                return REDIRECT_TYPE_SCRIPT;
            } else if (strcasecmp(r_type_str, "REDIRECT_TYPE_META_REFRESH")
                    == 0) {
                return REDIRECT_TYPE_META_REFRESH;
            } else {
                return REDIRECT_TYPE_UNKNOWN;
            }
        }

        bool readResult(const string & from, const char *result_line,
                std::string & result) 
        {
            char *from_url = NULL, *redirect = NULL, *to_url =
                NULL, *redirect_type = NULL;

            int ret = sscanf(result_line, "%as %as %as %as",
                    &from_url, &redirect, &to_url,
                    &redirect_type);

            bool is_redirected = false;
            if (ret >= 2
                    && from == from_url
                    && strcasecmp(redirect, "REDIRECT_TO") == 0
                    && ret == 4) {
                result = to_url;
                is_redirected = true;
            } 

            if(from_url) free(from_url);
            if(redirect) free(redirect);
            if(to_url) free(to_url);
            if(redirect_type) free(redirect_type);

            return is_redirected;
        }

#if 0
        bool findScriptRedirect(const URI& base, const char* html_page, size_t page_size,
                string& result_url)
        {
            URI result;
            bool have_result = false;

            uri base_uri;
            if (URI_to_uri(base, &base_uri))
            {
                uri result_uri;
                uri opener_result_uri;
                pthread_mutex_lock(&sr_mutex);
                int n = script_redirect_v2(html_page, page_size, &base_uri, &result_uri,
                        &opener_result_uri);
                pthread_mutex_unlock(&sr_mutex);
                if (n > 0)
                {
                    if (n & 2)
                    {
                        have_result = uri_to_URI(&result_uri, result);

                        uri_destroy(&result_uri);
                    }
                    if (n & 4)
                    {
                        URI opener_result;
                        uri_to_URI(&opener_result_uri,opener_result);

                        SSLOG_DEBUG("SCRIPT_OPENER_REDIRECT FROM %s TO %s\n",
                                base.ToString().c_str(),
                                opener_result.ToString().c_str());
                        uri_destroy(&opener_result_uri);
                    }
                }

                uri_destroy(&base_uri);
            }
            if (have_result && HttpUriNormalize(result)) 
            {
                result.ToString(result_url);
                return result_url != base.ToString();
            }

            return false;
        }
#endif
};


TRedirectChecker::TRedirectChecker() :impl_(new Impl()) 
{
} 

TRedirectChecker::~TRedirectChecker() 
{
    delete impl_;
}

bool TRedirectChecker::checkScriptRedirect(const std::string & from, const Response & resp, std::string & result) 
{
    return impl_->checkScriptRedirect(from, resp, result);
}

bool TRedirectChecker::checkMetaRedirect(const std::string & from, const Response & resp, std::string & result) 
{
    return impl_->checkMetaRedirect(from, resp, result);
}
