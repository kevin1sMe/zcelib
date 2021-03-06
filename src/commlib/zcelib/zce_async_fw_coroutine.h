

#ifndef ZCE_LIB_ASYNC_FRAMEWORK_COROUTINE_
#define ZCE_LIB_ASYNC_FRAMEWORK_COROUTINE_


#include "zce_async_fw_base.h"

//====================================================================================


/*!
* @brief      协程对象
*             注意，为了避免一些无意义的暴漏，我这儿选择的继承方式是private
*/
class ZCE_Async_Coroutine :private ZCE_Async_Object
{
    friend class ZCE_Async_CoroutineMgr;

    //
    enum COROUTINE_STATE
    {
        //携程传递给管理器的状态值，
        COROUTINE_CONTINUE = 0x10001,
        COROUTINE_EXIT = 0x10002,
        
        //超时后，管理器通知携程的状态值
        COROUTINE_TIMEOUT = 0x20002,
    };

    
public:
    /*!
    * @brief      构造函数，
    * @param      async_mgr ,协程异步管理器的指针
    */
    ZCE_Async_Coroutine(ZCE_Async_CoroutineMgr *async_mgr);
protected:
    /*!
    * @brief      析构函数
    */
    ~ZCE_Async_Coroutine();

public:

    /*!
    * @brief      初始化函数，在构造函数后调用，在放入池子前执行一次，
    * @return     int 0标识成功
    */
    virtual int initialize(unsigned int reg_cmd);

    /*!
    * @brief      结束销毁函数，在析构前的调用
    * @return     int
    */
    virtual int finish();

protected:    

    ///协程对象的运行函数
    void coroutine_do();

    ///协程运行,你要重载的函数
    virtual int coroutine_run() = 0;
    
    ///切换回Main，协程还会继续运行
    void yeild_main_continue();

    ///切换回Main,协程退出
    void yeild_main_exit();

    ///切换回协程，也就是切换到他自己运行
    void yeild_coroutine();


    /*!
    * @brief      等待time_out 时间后超时，设置定时器后，切换协程到main
    * @return     int
    * @param      time_out
    */
    int waitfor_timeout(const ZCE_Time_Value &time_out);


    /*!
    * @brief      继承ZCE_Async_Object的函数，
    * @param[out] continue_run 返回参数，返回当前的协程是否要继续运行下去
    */
    virtual void on_run(bool &continue_run);
    
    /*!
    * @brief      异步对象超时处理
    * @param[in]  now_time  发生超时的时间，
    * @param[out] continue_run 异步对象是否继续运行,
    */
    virtual void on_timeout(const ZCE_Time_Value & now_time,
        bool &continue_run);



protected:
    
    ///最小的堆栈
    static const size_t MIN_STACK_SIZE = 16 * 1024;
    ///默认堆栈
    static const size_t DEF_STACK_SIZE = 64 * 1024;
    ///最大的堆栈
    static const size_t MAX_STACK_SIZE = 256 * 1024;

public:

    ///static 函数，用于协程运行函数，调用协程对象的运行函数
    static void static_do(ZCE_Async_Coroutine *coroutine);

protected:

    ///最小的堆栈
    size_t min_stack_size = 16 * 1024;
    
    ///协程对象
    coroutine_t      handle_;

    ///协程的堆栈大小，
    size_t           stack_size_;

    ///协程的状态
    COROUTINE_STATE  coroutine_state_;
};

//====================================================================================

/*!
* @brief      协程对象主控管理类
*             
*/
class ZCE_Async_CoroutineMgr :public ZCE_Async_ObjectMgr
{
public:

    //
    ZCE_Async_CoroutineMgr(ZCE_Timer_Queue *timer_queue);
    virtual ~ZCE_Async_CoroutineMgr();

protected:
    
    ///默认异步对象池子的初始化的数量
    static const size_t COROUTINE_POOL_INIT_SIZE = 1;
    ///默认池子扩展的时候，扩展的异步对象的数量
    static const size_t COROUTINE_POOL_EXTEND_SIZE = 16;

};


#endif //#ifndef ZCE_LIB_ASYNC_FRAMEWORK_COROUTINE_

