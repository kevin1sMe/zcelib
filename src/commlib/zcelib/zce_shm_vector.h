/*!
* @copyright  2004-2013  Apache License, Version 2.0 FULLSAIL
* @filename   zce_shm_vector.h
* @author     Sailzeng <sailerzeng@gmail.com>
* @version
* @date       2005年10月21日
* @brief      在共享内存中使用的vector，其实就是数组（向量）了，
*             但是没有动态扩展特性，这个见谅，因为我毕竟在一块内存里面
*             干活，扩展的事情，亲那是你的！
* 
* @details
* 
*/

#ifndef ZCE_LIB_SHM_VECTOR_H_
#define ZCE_LIB_SHM_VECTOR_H_

#include "zce_shm_predefine.h"

namespace ZCE_LIB
{

///共享内存vector头部数据区
class _shm_vector_head
{
protected:

    //头部构造函数
    _shm_vector_head():
        size_of_mmap_(0),
        num_of_node_(0),
        num_of_use_(0)
    {
    }
    //析构函数
    ~_shm_vector_head()
    {
    }

public:

    ///内存区的长度
    size_t               size_of_mmap_;
    ///结点总数，
    size_t               num_of_node_;

    ///表示目前使用的结点个数
    size_t               num_of_use_;

};

/*!
* @brief      共享内存中使用的vector，彻底简化版本
* @tparam     _value_type  数组类型
*/
template <class _value_type> class shm_vector:
    public _shm_memory_base
{
public:

    ///定义迭代器,这个简单
    typedef _value_type *iterator;

protected:

    //只定义,不实现,
    const shm_vector<_value_type> & operator=(const shm_vector<_value_type> &others);

    ///默认构造函数,就是不给你用
    shm_vector():
        _shm_memory_base(NULL),
        data_base_(NULL)
    {
    }

public:

    /*!
    * @brief      构造函数，根据参数进行初始化，
    * @return     void
    * @param      numnode
    * @param      pmmap
    * @param      if_restore
    */
    shm_vector(const size_t numnode, char *pmmap, bool if_restore = false):
        _shm_memory_base(pmmap),
        data_base_(NULL)
    {
        initialize(numnode, pmmap, if_restore);
    }
    ///析构函数
    ~shm_vector()
    {
    }
public:

    ///内存区的构成为 定义区,data区,返回所需要的长度,
    static size_t getallocsize(const size_t numnode)
    {
        return  sizeof(_shm_vector_head)  + sizeof(_value_type) * (numnode ) ;
    }

    ///初始化
    static shm_vector<_value_type>* initialize(const size_t numnode, char *pmmap, bool if_restore = false)
    {

        _shm_vector_head *vectorhead  = reinterpret_cast<_shm_vector_head *>(pmmap);

        //如果是恢复,数据都在内存中,
        if (if_restore == true)
        {
            //检查一下恢复的内存是否正确,
            if (getallocsize(numnode) != vectorhead->size_of_mmap_ ||
                numnode != vectorhead->num_of_node_ )
            {
                return NULL;
            }
        }

        //初始化尺寸
        vectorhead->size_of_mmap_ = getallocsize(numnode);
        vectorhead->num_of_node_ = numnode;

        shm_vector<_value_type>* instance = new shm_vector<_value_type>();

        //所有的指针都是更加基地址计算得到的,用于方便计算,每次初始化会重新计算
        instance->smem_base_ = pmmap;
        instance->vector_head_ = vectorhead;
        instance->data_base_  = reinterpret_cast<_value_type *>(pmmap + sizeof(_shm_vector_head) );

        if (if_restore == false)
        {
            instance->clear();
        }

        //打完收工
        return instance;
    }

    ///
    void clear()
    {
        vector_head_->num_of_use_ = 0;
    }

    ///用[]访问数据，越界了自己负责
    _value_type &operator[](size_t n)
    {
        return *(data_base_ + n);
    }

    ///开始位置的迭代器
    iterator begin()
    {
        return data_base_;
    }
    ///结束位置的迭代器
    iterator end()
    {
        return data_base_ + vector_head_->num_of_use_;
    }

    ///是否为满和空
    bool empty() const
    {
        return (vector_head_->num_of_use_ == 0);
    }
    ///
    bool full() const
    {
        return (vector_head_->num_of_use_ == vector_head_->num_of_node_);
    }

    ///重新设置空间，可以增大和缩小
    void resize(size_t num)
    {
#if defined _DEBUG || defined DEBUG
        assert(num <= vector_head_->num_of_node_);
#endif

        //如果是扩大空间，生产数据
        if (num > vector_head_->num_of_use_)
        {
            //生产默认的数据
            for (size_t i = vector_head_->num_of_use_; i < num; ++i)
            {
                new (data_base_ + i) _value_type();
            }
        }
        //如果是缩小空间,销毁数据，调用析构
        else if (num < vector_head_->num_of_use_)
        {
            //生产默认的数据,析构
            for (size_t i = num; i < vector_head_->num_of_use_; ++i)
            {
                (data_base_ + i)->~_value_type();
            }
        }
        //等于，什么都不做
        else
        {

        }

        vector_head_->num_of_use_ = num;
    }

    //使用了的空间的数量
    size_t size() const
    {
        return vector_head_->num_of_use_;
    }
    //返回链表池子的容量
    size_t capacity() const
    {
        return vector_head_->num_of_node_;
    }
    //剩余空间的容量
    size_t sizefreenode() const
    {
        return vector_head_->num_of_node_ - vector_head_->num_of_use_;
    }

    ///关键位置
    _value_type &front()
    {
        return data_base_;
    }
    ///
    _value_type &back()
    {
        return *(data_base_ + ( vector_head_->num_of_use_ - 1));
    }

    ///向后添加数据
    bool push_back(const _value_type &val)
    {
        if (vector_head_->num_of_use_ == vector_head_->num_of_node_)
        {
            return false;
        }

        //使用placement new 复制对象
        new (data_base_ + vector_head_->num_of_use_) _value_type(val);

        ++(vector_head_->num_of_use_);

        return true;
    }

    ///从后面删除数据
    bool pop_back()
    {
        if ( vector_head_->num_of_use_ == 0 )
        {
            return false;
        }

        //显式调用析构函数
        (data_base_ + vector_head_->num_of_use_)->~_value_type();

        --(vector_head_->num_of_use_);
        return true;
    }

protected:
    ///
    _shm_vector_head   *vector_head_;
    ///数据区起始指针,
    _value_type         *data_base_;

};

};

#endif //ZCE_LIB_SHM_VECTOR_H_

