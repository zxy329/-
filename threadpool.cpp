#include"threadpool.h"

const int TASK_MAX_THRESHOLD=1024;
const int Thread_MAX_EHRESHOLD=10;
const int THREAD_MAX_IDLE_TIME=10;
//秒
///////////////线程池方法实现////////////////

ThreadPool::ThreadPool()
   :initThreadSize_(0),
    taskSize_(0),
    taskQueMaxTresHold_ (TASK_MAX_THRESHOLD),
    poolMode_(PoolMode::MODE_FIXED),
    isPoolRunning_(false),
    idleThreadSize_(0),
    threadSizeThresHold_(Thread_MAX_EHRESHOLD),
    curThreadSize_(0)
    {}

ThreadPool::~ThreadPool()
{
    isPoolRunning_=false;
    //等待线程池里所有的线程返回  有两种状态： 阻塞&正在执行任务中
   
    //解决了线程池中线程先获取锁产生的死锁问题（taskQueMutex_）
    std::unique_lock<std::mutex>lock (taskQueMutex_);
     notEmpty_.notify_all();
    exitCond_.wait(lock,[&]()->bool{return threads_.size()==0;});

}

bool ThreadPool:: checkRunningState()const
{
    return isPoolRunning_;
}

void ThreadPool::start(int initThreadSize)
{
    isPoolRunning_=true;
    initThreadSize_=initThreadSize;
    curThreadSize_=initThreadSize;

    for(int i=0;i<initThreadSize_;i++)
    {
        //创建线程对象，把线程函数给到线程对象
        auto ptr=std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        //threads_.emplace_back(std::move(ptr));//unique_ptr的拷贝构造函数被删除  资源转移
        int threadid=ptr->getId();
        threads_.emplace(threadid,std::move(ptr));
    }
    for(int i=0;i<initThreadSize_;i++)
    {
        threads_[i]->start();
        idleThreadSize_++;//每启动一个线程，即可用的线程数量加一
        
    }
}

void ThreadPool::setMode(PoolMode mode)
{
    if(checkRunningState())   
    return;
    poolMode_=mode;
}
    
void ThreadPool::setTaskQueMaxTresHold(int threshold)
{
    if(checkRunningState())
    return;
    taskQueMaxTresHold_=threshold;
}
void ThreadPool::setThreadSizeThresHold(int threshold)
{
    if(checkRunningState())
    return;
    if(poolMode_==PoolMode::MODE_CACHED)
    {
       threadSizeThresHold_=threshold;

    }
    
}

Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    std::unique_lock<std::mutex> lock(taskQueMutex_);
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),[&]()->bool{return taskQue_.size()<(size_t)taskQueMaxTresHold_;}))
    {
        std::cerr<<"task queue is full,submit task fail"<<std::endl;
        return Result(sp,false); 

    }

    taskQue_.emplace(sp);
    taskSize_++;
    notEmpty_.notify_all();

    //cached模式 场景：小而快的任务
    if(poolMode_==PoolMode::MODE_CACHED&&idleThreadSize_<taskSize_&&curThreadSize_<threadSizeThresHold_)
    {
       auto ptr=std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this,std::placeholders::_1));
        //threads_.emplace_back(std::move(ptr));//unique_ptr的拷贝构造函数被删除  资源转移
        int threadid=ptr->getId();
        threads_.emplace(threadid,std::move(ptr));
        threads_[threadid]->start();//启动创建的新线程
        curThreadSize_++; 
        idleThreadSize_++;
    }
    return Result(sp);
  
}
void ThreadPool::threadFunc(int threadid)
{
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime=std::chrono::high_resolution_clock::now();
    //修改线程池资源回收方法，使所有任务执行完后，线程池才可以回收所有线程
    for(;;)
    {
        

        std::shared_ptr<Task> task;
        {
        std::unique_lock<std::mutex> lock (taskQueMutex_);

             //cached 模式下，空闲时间超过60s 应该把超过initTreadSize_的线程释放掉
             //当前时间-上一次线程执行的时间>60s
            //每一秒钟返回一次

            //锁加双重判断      *****解决了main函数先获取锁而产生的死锁问题
            while(taskSize_== 0)
            {   
                //线程池要结束，回收线程资源
                if(!isPoolRunning_)
                {
                    threads_.erase(threadid);
                        
                    std::cout<<"threadid"<<std::this_thread::get_id()<<"exit"<<std::endl;
                    exitCond_.notify_all();
                    return ;//线程函数结束，线程结束
    
                }
         
                if(poolMode_==PoolMode::MODE_CACHED)
                {
                    //条件变量 超时返回
                if(std::cv_status::timeout==notEmpty_.wait_for(lock,std::chrono::seconds(1)))
                {
                   std::chrono::time_point<std::chrono::high_resolution_clock> now=std::chrono::high_resolution_clock::now();
                 
                    auto dur=std::chrono::duration_cast<std::chrono::seconds>(now-lastTime);
                    if(dur.count()>=THREAD_MAX_IDLE_TIME&&curThreadSize_>initThreadSize_)
                    {
                        //开始回收线程
                        //记录线程数量的相关变量的值修改
                        //把线程对象从线程列表中删除
                        //threadid->thread对象->删除
                        threads_.erase(threadid);
                        curThreadSize_--;
                        idleThreadSize_--;
                        std::cout<<"threadid"<<std::this_thread::get_id()<<"exit"<<std::endl;
                        return;

                    }
                }
                }
                else
                {
                     notEmpty_.wait(lock);
                     
                }
                //线程池要结束，回收线程资源
              // if(!isPoolRunning_)
               //{
                  //threads_.erase(threadid);
                        
                 // std::cout<<"threadid"<<std::this_thread::get_id()<<"exit"<<std::endl;
                 // exitCond_.notify_all();
                  //return;

              // }
            
             
         } 
         
        
        idleThreadSize_--;
        task=taskQue_.front();
        taskQue_.pop();
        taskSize_--;
        
        if(taskSize_>0)
        {
          notEmpty_.notify_all();  
        }
        notFull_.notify_all();
        }
        if(task!=nullptr)
        {
            task->exec();
        }
        lastTime=std::chrono::high_resolution_clock().now();
        idleThreadSize_++;
        

    }
   

}


////////////////线程方法实现///////////////
int Thread::generatedId_=0;
Thread::Thread(ThreadFunc func):func_(func),threadId_(generatedId_++)
{
    
}
Thread::~Thread()
{
    
}
int Thread::getId()const
{
  return  threadId_;
}

void Thread::start()
{
    std::thread t(func_,threadId_);
    t.detach();      //线程分离，执行线程函数
}

///////////Result实现方法


    Result:: Result(std::shared_ptr<Task>task,bool isVaild)
    {
      task_=task;
      isVaild_=isVaild;
      task_->setResult(this);

    }
    Any Result::get()
    { 
        if(!isVaild_)
        {
            return "";
        }
      sem_.wait();
      return std::move(any_);
    }
    void Result::setValue(Any any)
    {
      any_=std::move(any);
      sem_.post();
    }

///////////Task实现方式
   void Task::exec()
   {
     result_->setValue(run());

   }
   Task::Task():result_(nullptr)
   {

   }
   void Task::setResult(Result* result)
    {

        result_=result;
    }