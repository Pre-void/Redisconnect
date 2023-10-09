#include <iostream>
#include "string"
#include "Redisconnect_myself.h"

int main() {
    std::string val;

    /**初始化连接池**/
    RedisConnect::Setup("127.0.0.1",6379,"123456");

    /**从连接池中获取一个连接**/
    shared_ptr<RedisConnect> redis = RedisConnect::Instance();

    /**设置一个键值**/
    redis->set("name","lzh");


    /**获取键值内容**/
    redis->get("name",val);

    std::cout << val << std::endl;


    /**执行expire命令设置超时时间**/
    redis->execute("expire","name",60);

    /**获取超时时间**/
    redis->execute("ttl","name");

    /**调用getStatus方法获取ttl命令执行的结果**/
    std::cout << "超时时间：" << redis->getStatus() << std::endl;

    /**执行del命令删除键值**/
    redis->execute("del","name");

    /**获取分布式锁**/
    if (redis->lock("lockey")){
        std::puts("获取分布式锁成功");

        /**释放分布式锁**/
        if (redis->unlock("释放分布式锁成功")){
            std::puts("释放分布式锁成功");
        }


    }
    return 0;
}
