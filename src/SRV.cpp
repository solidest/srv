
/*
**
** Server.cpp
** 提供网络api服务
** 管理box客端的接入
** 对客户端传入的命令进行调度
** 服务器相关的命令实现
**
*/
#include <unistd.h>
#include <unordered_map>
#include <atomic>
#include <sstream>
#include <iostream>
#include "SRV.h"
#include "CTX.h"

//inner object
static uv_loop_t* _mainloop;
static uv_tcp_t* _server;

unordered_map<string, CommandInfo*> _cmdsTable;

struct CommandInfo cmdTable[] = {
    {"ping", cmdPing, WORK_TYPE_ALL },
    {"state", cmdState, WORK_TYPE_ALL },
    {"exit", cmdExit, WORK_TYPE_ALL },
    {"prepear", cmdPrepare, WORK_TYPE_IDLE | WORK_TYPE_PREPARED },
    {"start", cmdStartCase, WORK_TYPE_PREPARED },
    {"pause", cmdPause, WORK_TYPE_RUNNING },
    {"continue", cmdContinue, WORK_TYPE_PAUSE },
    {"stop", cmdStopCase, WORK_TYPE_RUNNING | WORK_TYPE_PAUSE }
};


//============================== initial ==============================
//启动
void StartServer(const char* ip, unsigned short port)
{
    //initial commands talbe
    int numcommands = sizeof(cmdTable)/sizeof(struct CommandInfo);
    for(int i = 0; i < numcommands; i++) {
        struct CommandInfo *cmdi = cmdTable+i;
        _cmdsTable.insert(pair<string, CommandInfo*>(cmdi->name, cmdi));
    }

    //initial serve
    _mainloop = uv_default_loop();
    _server = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

    struct sockaddr_in addr;
    uv_tcp_init(_mainloop, _server);
    uv_ip4_addr(ip, port, &addr);
    uv_tcp_bind(_server, (const struct sockaddr*)&addr, 0);
    cout << "start listen on " << ip << ":" << port << endl;
    int res = uv_listen((uv_stream_t*)_server, 5, OnClientConnect);
    if(res) {
        throw runtime_error(uv_strerror(res));
    }

    //run server
    uv_run(_mainloop, UV_RUN_DEFAULT);

    //free server
    free(_server);
    //cout << "exit" << endl;

    //TODO stop other thread
}

//close and release client
void CloseClient(Client* c) {
    uv_close((uv_handle_t*)c->Handle, OnClientClosed);
}


//============================ for uv =================================
//客户端连接已经关闭
void OnClientClosed(uv_handle_t *c) {
    auto client = (Client*)c->data;
    delete client;
    free(c);
    //cout << "client closed" << endl;
}

//有客户端连入
void OnClientConnect(uv_stream_t *s, int status) {
    if(status<0) {
        throw runtime_error(uv_strerror(status));
    }

    auto h = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(_mainloop, h);
    auto res = uv_tcp_keepalive(h, 1, 0);
    if(res!=0) {
        throw runtime_error(uv_strerror(status));
    }
    res = uv_tcp_nodelay(h, 1);
    if(res!=0) {
        throw runtime_error(uv_strerror(status));
    }

    auto client = new Client((uv_stream_t*)h);
    h->data = client;

    res = uv_accept(s, (uv_stream_t*)client->Handle);
    if (res==0)  {
        uv_read_start((uv_stream_t*)client->Handle, OnNewBuffer, OnRead);
    }
    else {
        uv_close((uv_handle_t*)client->Handle, OnClientClosed);
        throw runtime_error(uv_strerror(status));
    }
}

//分配内存
void OnNewBuffer(uv_handle_t *c, size_t suggested_size, uv_buf_t *buf) {
    //cout << "new buff: " << suggested_size << endl;

    auto s = sdsMakeRoomFor(sdsempty(), 2048);
    buf->base = s;
    buf->len = sdsavail(s);
}

//读取数据
void OnRead(uv_stream_t *c, ssize_t nread, const uv_buf_t *buf) {
    
    if (nread < 0)  {
        if (nread != UV_EOF){
            throw runtime_error(uv_strerror(nread));
        }
        uv_close((uv_handle_t*)c, OnClientClosed);
    }
    else if(nread>0) {

        //freed recved data to client object
        auto client = (Client*)c->data;
        auto ns = (sds)buf->base;
        //cout << "read: " << ns << endl;
        sdssetlen(ns, nread);
        client->FeedRecvBuf(ns);
        sdsfree(ns);

        //get command and execute it
        auto cmd = client->NextCmdInfo();
        while(cmd != NULL) {
            ExecCommand(*client, *cmd);
            client->PopOneCmdInfo();
            cmd = client->NextCmdInfo();
        }

        //reply to remote client
        if(client->ReplySize()>0)  {
            auto h = (uv_stream_t*)client->Handle;
            auto buff = client->FlushReplyBuf();
            auto len = sdslen(buff);
            auto req = (uv_write_t*)malloc(sizeof(uv_write_t));
            auto bs = uv_buf_init(buff, len);
            req->data = buff;
            uv_write(req, h, &bs, 1, OnAfterSend);
        }
    }
}

//发送完毕 进行清理
void OnAfterSend(uv_write_t* req, int status) {
    //cout << "sened" << endl;
    if(status!=0)
        throw runtime_error("send error");
    sdsfree((sds)req->data);
    free(req);
}


//========================== command ===================================

//执行命令
void ExecCommand(Client& c, vector<sds>& ci) {
    auto cmdi = _cmdsTable.find(ci.front());
    if(cmdi != _cmdsTable.end()) {
        auto cmd = cmdi->second;
        if((cmd->allowMode & CTX::WorkType()) == 0) {
            char buff[800];
            sprintf(buff, "invalid command, because stBox state is %s", CTX::WorkTypeStr());
            throw runtime_error(buff);
        }

        try {
            cmd->cmd(c, ci);
        }
        catch(const std::exception& e) {
            c.AppendReplyBufError(e.what());
        }
    }
    else {
        cmdNil(c, ci);
    }
}

//不支持的命令
void cmdNil(Client& c, vector<sds>& ci) {
    c.AppendReplyBuf("-not supported command : %S (%u)\r\n", ci.front(), (unsigned int)ci.size()-1);
}

//ping
void cmdPing(Client& c, vector<sds>& ci) {
    CheckArgSize(1);
    c.AppendReplyBuf("+%s\r\n", "pong");
}

//state
void cmdState(Client& c, vector<sds>& ci) {
    CheckArgSize(1);
    sds args[] = {sdsnew(CTX::WorkTypeStr()), sdscatfmt(sdsempty(), "%i", CTX::WorkProjectId())};
    c.AppendReplyBuf(args, 2);
    sdsfree(args[0]);
    sdsfree(args[1]);
}

//exit all
void cmdExit(Client& c, vector<sds>& ci) {
    uv_close((uv_handle_t*)c.Handle, OnClientClosed);
    uv_stop(_mainloop);
}

void cmdPrepare(Client& c, vector<sds>& ci) {
    c.AppendReplyBufOk();
}

void cmdStartCase(Client& c, vector<sds>& ci) {
     c.AppendReplyBufOk();
}

void cmdPause(Client& c, vector<sds>& ci) {
    c.AppendReplyBufOk();
}

void cmdContinue(Client& c, vector<sds>& ci) {
    c.AppendReplyBufOk();
}

void cmdStopCase(Client& c, vector<sds>& ci) {
    c.AppendReplyBufOk();
}
