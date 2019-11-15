
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "src/SRV.h"
#include "hiredis/hiredis.h"

using namespace std;

static int tests = 0, fails = 0;
#define test(_s) { printf("#%02d ", ++tests); printf(_s); }
#define test_cond(_c) if(_c) printf("\033[0;32mPASSED\033[0;0m\n"); else {printf("\033[0;31mFAILED\033[0;0m\n"); fails++;}


void test_cmd_ping(redisContext* srv) {
    test("test command ping ");
    auto reply = (redisReply*)redisCommand(srv, "ping");
    test_cond(strcmp(reply->str, "pong")==0);
    freeReplyObject(reply);
}

void test_cmd_exit(redisContext* srv) {
    test("test command exit ");
    auto reply = (redisReply*)redisCommand(srv, "exit");
    test_cond(reply==nullptr);
}

void test_cmd_nil(redisContext* srv) {
    test("test command not supported ");
    auto reply = (redisReply*)redisCommand(srv, "djfl %s %s", "adffd", "dfafda");
    //cout << reply->str << endl;
    test_cond(reply->type==REDIS_REPLY_ERROR);
}

void test_cmd_state(redisContext* srv) {
    test("test command state ");
    auto reply = (redisReply*)redisCommand(srv, "state");
    //cout << reply->str << endl;
    test_cond(reply->elements==2 && strcmp(reply->element[0]->str, "idle")==0 && strcmp(reply->element[1]->str, "-1")==0);
}

void *loop(void* arg)
{
    StartServer("0.0.0.0", 1210);
    sleep(1);
    pthread_exit(NULL);
}

int main(int argcs, char **argvs) {
    pthread_t tid;
    auto ret = pthread_create(&tid,NULL,loop,NULL);
    sleep(1);

    auto srv = redisConnect("127.0.0.1", 1210);

    test_cmd_ping(srv);
    test_cmd_nil(srv); 
    test_cmd_state(srv);
    
    test_cmd_exit(srv);
    redisFree(srv);

    pthread_join(tid, NULL);
    return 0;
}

int main1(int argcs, char **argvs) {
    StartServer("0.0.0.0", 1210);
    return 0;
}
