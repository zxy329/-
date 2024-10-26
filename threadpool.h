#ifndef THREADPOOL_H
#define  THREADPOOL_H

#include<iostream>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<thread>
#include<unordered_map>
#include<chrono>
//any类
class Any
{
public:
Any()=default;
~Any()=default;
Any(const Any&)=delete;
Any& operator=(const Any&)=delete;
Any(Any&&)=default;
Any& operator=(Any&&)=default;


template<typename T>
  Any(T data):Base_(std::make_unique<Derive<T>>(data))
  {}

template<typename T>
  T cast_()
  {
    Derive<T>* ptr=dynamic_cast<Derive<T>*>(Base_.get());
    if(ptr==nullptr)
    {
        throw "type is dismatched";
    }
    return ptr->data_;

  }

private:
  class Base
  {
    public:
    virtual ~Base()=default;

  };
  template<typename T>
  class Derive:public Base
  {
    public:
    Derive(T data):data_(data)
    {}
    

    T data_;

  };
private:
  std::unique_ptr<Base> Base_;

};

//信号量类
class Semaphore
{
  public:
  Semaphore(int size=0):size_(size),isExit_(false)
  {}
  ~Semaphore()
  {
    isExit_=true;
  }
   void wait()
   {
    if(isExit_)
    return;
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [&]()->bool{return size_ > 0;});
    size_--;
   }
   void post()
   {
    if(isExit_)
    return;
    std::unique_lock<std::mutex> lock(mutex_);
    size_++;
    cond_.notify_all(); //  1.等待状态  2.释放mutex锁
    }
   private:
  int size_;
  std::atomic_bool isExit_; 
  std::mutex mutex_;
  std::condition_variable cond_;

};

//result类型
class Task;
class Result
{
    public:
       Result(std::shared_ptr<Task> task,bool isVaild=true);
       ~Result()=default;
       void setValue(Any any);
       Any get();
    private:
    Any any_;
    Semaphore sem_;
    std::shared_ptr<Task> task_;
    std::atomic_bool isVaild_;

};
//任务抽象基类
class Task
{
    public:
    Task();
    ~Task()=default;
    void exec();
    void setResult(Result* result);
    
    virtual Any run()=0;
    private:
    Result* result_;
    
};

//线程池模式
enum  class PoolMode
{
  MODE_FIXED,//固定数量的线程
  MODE_CACHED //线程数量可动态增长
 
};







//线程类型
class Thread
{
public:
    using ThreadFunc=std::function<void(int)>;
    Thread(ThreadFunc func);
    ~Thread();
    void start();
    int getId()const;


private:
    ThreadFunc func_;
    static int generatedId_;
    int threadId_;




};


//线程池类型
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();
    void start(int initThreadSize=4);
    void setMode(PoolMode mode);
    
    void setTaskQueMaxTresHold(int threshold);
    void setThreadSizeThresHold(int threshold);
    Result submitTask(std::shared_ptr<Task> sp);
    ThreadPool (const ThreadPool&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;
private:
    void threadFunc(int threadid);

    bool checkRunningState()const;
private:
    //std::vector<std::unique_ptr<Thread>> threads_;
    std::unordered_map<int,std::unique_ptr<Thread> > threads_;
    int threadSizeThresHold_;
    int initThreadSize_;
    std::atomic_int curThreadSize_;//记录当前线程的数量
    std::atomic_int idleThreadSize_;//空闲线程数量

    std::queue<std::shared_ptr<Task>>taskQue_;
    std::atomic_int taskSize_;
    
    int taskQueMaxTresHold_ ;

    std::mutex taskQueMutex_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    std::condition_variable exitCond_;//等待线程资源全部被回收

    PoolMode poolMode_;

    std::atomic_bool isPoolRunning_;
};


#endif