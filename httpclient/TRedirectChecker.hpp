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

struct RedirectInfo
{
    std::string to_url;
    REDIRECT_TYPE type;
};

class TRedirectChecker
{
    public:
        /// Return true if redirected,
        /// @param to is the result of redirected.
        /// 
        /// @note assume page is HTML page.

        bool checkScriptRedirect(const std::string &from, const Response &resp, std::string &result);
        bool checkMetaRedirect(const std::string &from, const Response &resp, std::string &result);

        TRedirectChecker();
        ~TRedirectChecker();

        static boost::shared_ptr<TRedirectChecker> &instance();
        static void instance(boost::shared_ptr<TRedirectChecker> &obj);

    private:
        class Impl;
        Impl *impl_;

        static boost::shared_ptr<TRedirectChecker> instance_;
};

#endif   /* ----- #ifndef _SPIDERSERVICE_TREDIRECTCHECKER_INC  ----- */
