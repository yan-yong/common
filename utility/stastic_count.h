#ifndef __STASTIC_COUNT_HPP
#define __STASTIC_COUNT_HPP

template<class T, int count>
class StasticCount
{
    T sum_;
    T val_array[count];
    unsigned idx_;
    unsigned stastic_count_;    

public:
    StasticCount(): sum_(T()), idx_(0), stastic_count_(0)
    {  
        assert(count > 0);
        for(int i = 0; i < count; i++)
            val_array[i] = T();
    }
    void Add(T val)
    {  
        idx_ = idx_++ % count;
        sum_ -= val_array[idx_];
        val_array[idx_] = val;
        sum_ += val;
        ++stastic_count_;
    }
    T Average() const
    {
        if(stastic_count_ == 0)
            return 0; 
        return sum_ / count;
    }
};

#endif
