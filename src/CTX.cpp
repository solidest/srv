
#include "sds.h"

#include <hiredis/async.h>
#include <hiredis/adapters/libuv.h>
#include <stdexcept>
#include <iostream>

#include "CTX.h"

using namespace std;

#define REDIS_UNIX_SOCKET "/var/run/redis/redis-server.sock"

vector<redisContext*> CTX::_redisConns;
redisAsyncContext* CTX::_asyncRedis = nullptr;
uv_loop_t* CTX::_mainLoop = nullptr;
atomic_ullong CTX::_now(0);
atomic_uint32_t CTX::_work_type(1);
int CTX::_maxStep = 0;
int CTX::_proj_id = -1;

//step timer
void CTX::Tick() {
    if(_now==0) {
        _now = uv_now(_mainLoop);
    } else {
        auto now = uv_now(_mainLoop);
        if(now-_now>_maxStep) {
            _maxStep = now-_now;
        }
        _now = now;
    }
}

//now timer
unsigned long long CTX::Now() {
    return _now;
}

//get work type string
const char* CTX::WorkTypeStr() {
    
    switch (_work_type)
    {
        case WORK_TYPE_IDLE:
            return "idle";

        case WORK_TYPE_PAUSE:
            return "pause";

        case WORK_TYPE_RUNNING:
            return "running";

        case WORK_TYPE_PREPARED:
            return "prepared";
    
        default:
            return "unknow";
    }
}


//log cvt value to db
static void afterLog(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = (redisReply *)r;
    if (reply == NULL || reply->integer<1) {
        throw runtime_error("log data to db error");
    } else {
        cout << reply->integer << endl;
        freeReplyObject(reply);
    }
}

void CTX::LogCVT(const string& name, const json& value) {
    const char* cmd = "LPUSH";

    sds key = sdsnew("$");
    key = sdscat(key, name.c_str());

    json vlog = json::object();
    vlog["t"] = (unsigned long long)_now;
    vlog["v"] = value;
    auto data = json::to_msgpack(vlog);

    const char* argv[] = {cmd, key, (const char*)data.data()};
    const size_t argLen[] = {strlen(cmd), sdslen(key), data.size()};

    redisAsyncCommandArgv(_asyncRedis, afterLog, nullptr, 3, argv, argLen);
    sdsfree(key);
}



static void connectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        throw runtime_error(c->errstr);
    }
}

static void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        throw runtime_error(c->errstr);
    }
}

void CTX::Initial(int workerThreadCount, uv_loop_t* mainLoop) {
    _mainLoop = mainLoop;
    _asyncRedis = redisAsyncConnectUnix(REDIS_UNIX_SOCKET);
    if (_asyncRedis->err) {
        throw runtime_error(_asyncRedis->errstr);
    }

    for(int i=0; i< workerThreadCount; i++) {
        redisContext *c = redisConnectUnix(REDIS_UNIX_SOCKET);
        if (c->err) {
            throw runtime_error(c->errstr);
        }
        _redisConns.push_back(c);
    }

    redisLibuvAttach(_asyncRedis, _mainLoop);
    redisAsyncSetConnectCallback(_asyncRedis,connectCallback);
    redisAsyncSetDisconnectCallback(_asyncRedis,disconnectCallback);
}

void CTX::Release() {
    redisAsyncDisconnect(_asyncRedis);
    for(int i=0; i< _redisConns.size(); i++) {
        redisFree(_redisConns[i]);
    }
}