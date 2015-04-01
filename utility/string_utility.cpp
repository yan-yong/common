#include <string_utility.h>

unsigned string_count(const char* src_cont, const char* pat)
{
    const char* cur_cont = src_cont;
    unsigned cnt = 0;
    unsigned cont_len = strlen(src_cont);
    unsigned pat_len = strlen(pat);
    const char* cur = strstr(cur_cont, pat); 
    while(cur)
    {
        cnt++;
        cur_cont = cur + pat_len;
        if(cur_cont < src_cont + cont_len)
            break;
        cur = strstr(cur_cont, pat);
    } 
    return cnt;
}

size_t split_string(const char * str, const char *sep, std::vector<std::string> &result)
{
    const char *p = str;
    const char *q = str;

    size_t sep_len = strlen(sep);

    while((q = strstr(p, sep))){
        result.push_back(std::string(p, q - p));
        p = q + sep_len;
    }

    if(p){
        result.push_back(p);
    }
    return result.size();
}
