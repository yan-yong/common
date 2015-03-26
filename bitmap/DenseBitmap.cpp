#include "DenseBitmap.h"

int DenseBitmap::initialize(int order, std::string file_name)
{
    m_fid = -1;
    m_order = order;
    if(order >= 40 || order <= 3){
        LOG_ERROR("[Bitmap] order exceed ordinary.\n");
        return -1;
    }
    m_bitmap_size = 0x01ll << (order - 3);
    m_block_size  = m_bitmap_size / 512;
    m_save_file = file_name;
    boost::trim(m_save_file);
    m_mem = calloc(m_bitmap_size, 1);
    if(!m_mem){
        LOG_ERROR("[Bitmap] malloc %zd bytes memory failed.\n", m_bitmap_size);
        return -1;
    }
    if(m_save_file.empty()){
        LOG_INFO("[Bitmap] use no save file.\n");
        m_init = true;
        return 0;
    }

    m_fid = open(m_save_file.c_str(), O_RDWR|O_CREAT, 0x644);
    if(m_fid < 0){
        LOG_ERROR("[Bitmap] open file %s error\n", m_save_file.c_str());
        free(m_mem);
        m_mem = NULL;
        return -1;
    }
    m_block_state = calloc(m_block_size, 1);
    if(!m_block_state){
        LOG_ERROR("[Bitmap] malloc %zd bytes memory failed.\n", m_block_size);
        m_fid = -1;
        free(m_mem);
        m_mem = NULL;
        return -1;
    }
    for(int i = 0; i < MUTEX_NUM; i++)
        pthread_mutex_init(m_block_locks+i, NULL);

    struct stat file_stat;
    if(fstat(m_fid, &file_stat) < 0) {
        LOG_ERROR("[Bitmap] fstate error: %s\n", strerror(errno));
        m_fid = -1;
        free(m_mem);
        m_mem = NULL;
        return -1;
    }
    if(file_stat.st_size == 0) {
        LOG_INFO("[Bitmap] create new file %s\n", m_save_file.c_str());
        if(!__write_header() || !__write_mem(0, m_bitmap_size)){
            m_fid = -1;
            free(m_mem);
            m_mem = NULL;
            free(m_block_state);
            m_block_state = NULL;
            return -1;
        }
        m_init = true;
        return 0;
    }
    if(!__read_header() || !__read_mem(0, m_bitmap_size)){
        LOG_ERROR("[Bitmap] load data from %s error.\n", m_save_file.c_str());
        m_fid = -1;
        free(m_mem);
        m_mem = NULL;
        free(m_block_state);
        m_block_state = NULL;
        return -1;
    }
    LOG_INFO("[Bitmap] load data from %s success.\n", m_save_file.c_str());
    m_init = true;
    return 0;
}

