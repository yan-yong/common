/*
 * =====================================================================================
 *
 *       Filename:  TRedirectChecker.hpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/14/2009 02:48:23 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guo Shiwei (), guoshiwei@sohu-rd.com
 *        Company:  sohu
 *
 * =====================================================================================
 */


#ifndef  __TREDIRECTCHECKER_INC
#define  __TREDIRECTCHECKER_INC

#include <string>
#include <boost/shared_ptr.hpp>
#include "httpparser/HttpMessage.hpp"
#include "SchedulerTypes.hpp"
#include "singleton/Singleton.h"

struct RedirectInfo
{
    std::string to_url;
    REDIRECT_TYPE type;
};

class TRedirectChecker
{
    DECLARE_SINGLETON(TRedirectChecker);
    public:
        /// Return true if redirected,
        /// @param to is the result of redirected.
        /// 
        /// @note assume page is HTML page.
#if 0
        bool checkScriptRedirect(const std::string &from, const Response &resp, std::string &result);
#endif
        bool checkMetaRedirect(const std::string &from, const Response &resp, std::string &result);

        ~TRedirectChecker();

    private:
        class Impl;
        Impl *impl_;
};

#endif   /* ----- #ifndef _SPIDERSERVICE_TREDIRECTCHECKER_INC  ----- */
