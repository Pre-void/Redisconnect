//
// Created by LZH on 2023/9/28.
//

#ifndef   RESPOOL_H
#define   RESPOOL_H
//////////////////////////////////////////////////////////////////////////////
#include "typedef.h"

#include <ctime>
#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <sstream>
#include <iostream>
#include <iterator>
#include <typeinfo>
#include <algorithm>
#include <functional>

using namespace std;

template<typename T> class ResPool
{
    /**内部类，用于管理资源池中的每个资源。**/
    class Data
    {
    public:
        int num;             /**表示资源被使用的次数。**/
        time_t utime;        /**表示资源最后一次被使用的时间。**/
        shared_ptr<T> data;  /**使用 shared_ptr 来保存资源对象。**/

        /**用于获取资源，并在每次获取资源时更新 utime 和 num。  **/
        shared_ptr<T> get()
        {
            utime = time(NULL);
            num++;

            return data;
        }
        /**用于获取资源，并在每次获取资源时更新 utime 和 num。**/
        Data(shared_ptr<T> data)
        {
            update(data);
        }
        /**用于更新 Data 对象的资源和重置 num 和 utime。**/
        void update(shared_ptr<T> data)
        {
            this->num = 0;
            this->data = data;
            this->utime = time(NULL);
        }
    };

protected:
    mutex mtx;    /**个互斥锁，用于在多线程环境中对资源池进行互斥访问。**/
    int maxlen;   /**资源池中的最大资源数量。**/
    int timeout;  /**资源的超时时间，在多长时间内没有被使用后将被清理。**/
    vector<Data> vec;  /**保存 Data 对象的向量，用于存储资源。**/
    function<shared_ptr<T>()> func; /****/

public:
    /**从资源池中获取资源对象 shared_ptr<T>**/
    shared_ptr<T> get()
    {
        /**timeout 若小于0表示不启用超时机制，直接通过 func() 调用创建资源对象并返回。**/
        if (timeout <= 0) return func();

        auto grasp = [&](){
            int len = 0;  /**连接池资源数**/
            int idx = -1; /**当前索引**/
            shared_ptr<T> tmp;  /**暂存**/
            time_t now = time(NULL);  /**获取当前时间**/

            mtx.lock();  /**加锁，以确保后续的操作是线程安全的。**/

            len = vec.size();
            /**遍历资源池中的资源对象：**/
            for (int i = 0; i < len; i++)
            {
                /**获取资源对象**/
                Data& item = vec[i];
                /**如果资源对象的指针为空（item.data.get() == NULL）或者只有当前 grasp 函数引用它（item.data.use_count() == 1）**/
                if (item.data.get() == NULL || item.data.use_count() == 1)
                {

                    if (tmp = item.data)
                    {
                        /**资源对象的使用次数小于 100 且超时时间内表示资源对象可以重用。**/
                        if (item.num < 100 && item.utime + timeout > now)
                        {
                            shared_ptr<T> data = item.get();

                            /**释放互斥锁，解锁资源池。**/
                            mtx.unlock();

                            return data;
                        }
                        /**将资源对象的item.data 置为空，表示该资源对象不可重用。**/
                        item.data = NULL;
                    }
                    /**记录当前可重用资源的索引。**/
                    idx = i;
                }
            }

            mtx.unlock();

            if (idx < 0)
            {
                /**首先检查资源池中的资源数量是否已经达到最大长度（len >= maxlen），如果是，
                 * 说明资源池已满，无法创建新的资源对象，因此直接返回一个空的 shared_ptr<T>()。**/
                if (len >= maxlen) return shared_ptr<T>();
                /**如果资源池未满，则通过调用 func() 函数创建一个新的资源对象，并将其赋值给 data。**/
                shared_ptr<T> data = func();
                /**如果新创建的资源对象为空，直接返回该对象。
                 * 这行代码 `if (data.get() == NULL) return data;` 的目的是检查通过 `func()` 创建的新资源对象是否为 `NULL`。
                 * 如果资源对象为 `NULL`，这意味着创建失败或出现了问题，因此没有必要将它插入到 `vec` 中，而是直接返回一个空的 `shared_ptr<T>`，
                 * 以指示获取资源失败。在这种情况下，将资源对象添加到 `vec` 中不会改变它为 `NULL` 的事实，因此可以避免不必要的操作和资源池的变化。**/
                if (data.get() == NULL) return data;

                mtx.lock();
                /**如果资源池的大小仍然小于最大长度（maxlen），则将新创建的资源对象 data 添加到资源池中。**/
                if (vec.size() < maxlen) vec.push_back(data);

                mtx.unlock();

                return data;
            }
            /**如果idx不为-1，说明idx索引处的索引可重用，先调用func()，若获取到的data为NULL，直接返回
             * 如果不为NULL，上锁更新后再解锁**/
            shared_ptr<T> data = func();

            if (data.get() == NULL) return data;

            mtx.lock();

            vec[idx].update(data);

            mtx.unlock();

            return data;
        };
        /**尝试调用 grasp() 函数获取资源，并将获取到的资源保存在 data 变量中**/
        shared_ptr<T> data = grasp();
        /**如果 data 为非空（即成功获取到资源），则直接返回 data，表示成功获取资源，可以在外部使用了。**/
        if (data) return data;
        /**如果第一次获取资源失败，设置一个截止时间 endtime，这个时间比当前时间晚 3 秒。然后进入一个无限循环，等待获取资源成功或者超过截止时间。**/
        time_t endtime = time(NULL) + 3;

        while (true)
        {   /**休眠 10 毫秒**/
            Sleep(10);
            /**检查 data 是否为有效的 shared_ptr。如果获取到资源（data 非空），则直接返回 data，表示成功获取资源。**/
            if (data = grasp()) return data;
            /**如果获取资源失败并且当前时间超过了截止时间 endtime，则退出循环，表示未能在规定时间内获取到资源。**/
            if (endtime < time(NULL)) break;
        }
        /**如果 data 为 NULL，则表示在规定时间内未能成功获取到资源。**/
        return data;
    }
    /**清空资源池**/
    void clear()
    {
        /**使用 lock_guard 对象 lk 锁住了 mtx，这是为了保证在清空资源池时不会被其他线程同时修改，确保线程安全。**/
        lock_guard<mutex> lk(mtx);

        vec.clear();
    }
    int getLength() const
    {
        return maxlen;
    }
    int getTimeout() const
    {
        return timeout;
    }
    void disable(shared_ptr<T> data)
    {
        lock_guard<mutex> lk(mtx);

        for (Data& item : vec)
        {
            if (data == item.data)
            {
                item.data = NULL;

                break;
            }
        }
    }
    void setLength(int maxlen)
    {
        lock_guard<mutex> lk(mtx);

        this->maxlen = maxlen;

        if (vec.size() > maxlen) vec.clear();
    }
    void setTimeout(int timeout)
    {
        lock_guard<mutex> lk(mtx);

        this->timeout = timeout;

        if (timeout <= 0) vec.clear();
    }
    void setCreator(function<shared_ptr<T>()> func)
    {
        lock_guard<mutex> lk(mtx);

        this->func = func;
        this->vec.clear();
    }
    ResPool(int maxlen = 8, int timeout = 60)
    {
        this->timeout = timeout;
        this->maxlen = maxlen;
    }
    ResPool(function<shared_ptr<T>()> func, int maxlen = 8, int timeout = 60)
    {
        this->timeout = timeout;
        this->maxlen = maxlen;
        this->func = func;
    }
};
//////////////////////////////////////////////////////////////////////////////
#endif