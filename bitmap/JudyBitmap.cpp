#include "JudyBitmap.h"

JudyBitmap::~JudyBitmap()
{
    Word_t present_num = 0;
    if(m_judy)
        J1FA(present_num, m_judy);
    if(m_fid){
        fclose(m_fid);
        m_fid = NULL;
    }
}

void JudyBitmap::rewrite_file()
{
    std::string temp_file = m_save_file + ".tmp";
    FILE* fid = fopen(m_save_file.c_str(), "w");
    if(!fid){
        LOG_ERROR("Error JudyBitmap::rewrite cannot create file %s\n", temp_file.c_str());
        exit(1);
    }
    if(m_fid)
        fclose(m_fid);
    m_fid = fid;

    std::string header = file_header();
    write(header.c_str(), header.size());

    int ret = 0;
    size_t bitmap_size = __size();
    m_cur_cnt = 0;
    Word_t cur = 0;
    for(size_t i = 0; i < bitmap_size && ret > 0; i++, cur++){
        J1F(ret, m_judy, cur);
        m_cur_cnt++;
        write_element(&cur, WORD_SIZE);
        if(i % 1000 == 0)
            LOG_INFO("JudyBitmap::rewrite_file rewriting %zd/%zd\n", i+1, bitmap_size);
    }
    rename(temp_file.c_str(), m_save_file.c_str());
    LOG_INFO("JudyBitmap::rewrite_file End: %zd bitmap items\n", bitmap_size);
}

int JudyBitmap::load_file(std::string file_name)
{   
    FILE* fid = fopen(file_name.c_str(), "r");
    if(!fid){
        LOG_INFO( "JudyBitmap::load_file: skip not exist file: %s\n", file_name.c_str());
        return 0;
    }

    fseek(fid, 0L, SEEK_END);
    m_file_size = ftell(fid);
    fseek(fid, 0L, SEEK_SET);
    if(m_file_size == 0){
        LOG_INFO( "JudyBitmap::load_file: skip empty file: %s\n", file_name.c_str());
        fclose(fid);
        return 0;
    }

    std::string header = file_header(); 
    char buf[1024];
    fgets(buf, 1024, fid);
    if(strncmp(header.c_str(), buf, strlen(JUDY_BITMAP_SAVE_HEADER)) != 0){
        LOG_ERROR( "JudyBitmap::load_file: read invalid header from %s\n", file_name.c_str());
        exit(1);
    }

    size_t data_item_num = (m_file_size - strlen(buf)) / WORD_SIZE;
    if(data_item_num == 0){
        LOG_INFO("JudyBitmap::load_file: no data in %s\n", file_name.c_str());
        return 0;
    }

    char* read_buf = new char[(WRITE_SIGN_INTERVAL+1)*WORD_SIZE];
    bool find_sign = false;
    size_t each_read_num = WRITE_SIGN_INTERVAL+1;
    while(true){
        size_t read_item_num = fread(read_buf, WORD_SIZE, each_read_num, fid);
        if(read_item_num == 0)
            break;
        /* begin find sign */
        if(find_sign){
            for(unsigned i = 0; i < read_item_num; i++){
                size_t * sign_ptr = (size_t*)(read_buf + i);
                if(*sign_ptr == SIGN_BYTES)
                {
                    size_t need_move_num = read_item_num - (i+1);
                    memmove(read_buf, read_buf + (i+1)*WORD_SIZE, need_move_num);
                    each_read_num = WRITE_SIGN_INTERVAL + 1 - need_move_num;
                    find_sign = false;
                    break;
                }
            }
            continue;
        }

        //test sign
        if(read_item_num == each_read_num){
            size_t * sign_ptr = (size_t*)(read_buf + WRITE_SIGN_INTERVAL*WORD_SIZE);
            if(*sign_ptr != SIGN_BYTES){
                find_sign = true;
                m_need_rewrite_file = true;
                LOG_ERROR("JudyBitmap::load_file: data sign is not fit, begin skip error data.");
                continue;
            }
            //subtract last sign types
            read_item_num -= 1;
        }
        //insert to bitmap
        for(unsigned i = 0; i < read_item_num; i++){
            size_t index = *(size_t*)(read_buf + i*WORD_SIZE);
            operate(index);
        }
        m_cur_cnt += read_item_num;
        LOG_INFO("JudyBitmap::load_file: loading %zd / %zd\n", m_cur_cnt, data_item_num);
    }

    delete[] read_buf;
    fclose(fid);
    return 0;
}

int JudyBitmap::initialize(int order, std::string file_name)
{
    WriteLocker lock(m_rw_lock);
    if(file_name.empty()){
        LOG_ERROR( "JudyBitmap::initialize: NULL file_name\n");
        exit(1);
    }

    m_save_file = file_name;
    if(load_file(file_name) != 0){
        LOG_ERROR("JudyBitmap::initialize: load %s failed.", file_name.c_str());
        return -1;
    }
    open_file();
    //data error or too many dumplicate will CAUSE file rewrite
    size_t cur_bitmap_size = __size();
    //m_need_rewrite_file = true;
    if(m_need_rewrite_file){
        LOG_INFO("JudyBitmap start rewrite %s to recover data error\n", m_save_file.c_str());
        rewrite_file();
    }
    if(m_cur_cnt && (!cur_bitmap_size || m_cur_cnt / cur_bitmap_size >= 2)){
        LOG_INFO("JudyBitmap start rewrite %s for less data file_num:%zd bitmap_num:%zd\n", 
                m_save_file.c_str(), m_cur_cnt, cur_bitmap_size);
        rewrite_file();
    }
    LOG_INFO("JudyBitmap initialize from %s success: %zd bitmap items.\n", file_name.c_str(), __size());
    return 0;
}


