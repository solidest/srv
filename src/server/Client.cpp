
/*
**
** Client.cpp
** 客户端连接对象
** 客户端api命令解析
** 客户端回复内容缓冲
**
*/

#include <assert.h>
#include <stdexcept>
#include "Client.h"

int Client::_count = 0;
uint64_t Client::_nextid = 1;

Client::Client(void* handle):_cmds(), _needRelease()
{
    _reqtype = REQUEST_TYPE_UNKNOW;
    _id = _nextid++;
    _count++;

    /* query */
    _querybuf = sdsnew(NULL);
    _qb_pos = 0;

    /* query parser */
    _next_len = -1;
    _next_count = -1;
    _args = new vector<sds>();

    /* Response buffer */
    _replybuf = sdsnew(NULL);

    /* client handle */
    Handle = handle;
}

Client::~Client()
{
    _count--;
    sdsfree(_querybuf);
    sdsfree(_replybuf);

    while(!_cmds.empty()) {
        ReleaseCmdInfo(_cmds.front());
        _cmds.pop_front();
    }

    while(!_args->empty()) {
        sdsfree(_args->back());
        _args->pop_back();
    }
    delete _args;

    assert(_needRelease.empty());
}

//清出回复缓冲区
sds Client::FlushReplyBuf() {
    auto ret = _replybuf;
    _replybuf = sdsnew(NULL);
    return ret;
}

//填充回复缓冲
void Client::FeedReplyBuf(sds buff) {
    _replybuf = sdscatsds(_replybuf, buff);
}

//size of reply buff
size_t Client::ReplySize() {
    return sdslen(_replybuf);
}

//释放命令调用信息
void Client::ReleaseCmdInfo(vector<sds>* ci)
{
    if(_reqtype == REQUEST_TYPE_TELNET) {
        sdsfreesplitres(_needRelease.front(), ci->size());
        _needRelease.pop_front();
        delete ci;        
    }
    else {
        for(auto& arg : *ci) {
            sdsfree(arg);
        }        
        delete ci;        
    }

}

//下一条需执行的命令
vector<sds>* Client::NextCmdInfo()
{
    if(!_cmds.empty())
        return _cmds.front();
    else
        return NULL;
}

//弹出一条已经执行的命令
void Client::PopOneCmdInfo()
{
    if(_cmds.size()==0) return;
    ReleaseCmdInfo(_cmds.front());
    _cmds.pop_front();
}

//填充接收到的buff
void Client::FeedRecvBuf(sds buff)
{
    if(_reqtype == REQUEST_TYPE_UNKNOW) {
        _reqtype = (buff[0] == '*' ? REQUEST_TYPE_CLIENT : REQUEST_TYPE_TELNET);
    }

    _querybuf = sdscatsds(_querybuf, buff);
    auto len = sdslen(_querybuf);

    //read until to end
    if(_reqtype == REQUEST_TYPE_CLIENT) {
        while(ReadCmdSegment(len));
    }
    else {
        ReadTelnetCommand(len);
    }

    //readed all query buff data
    if(_qb_pos == len) {
        if(sdslen(_querybuf)>2048) {
            sdsfree(_querybuf);
            _querybuf = sdsnew(NULL);
        } else {
            sdsclear(_querybuf);
        }
        _qb_pos = 0;
    }
}


//读取telnet命令
void Client::ReadTelnetCommand(int stopPos)
{
    if(stopPos>10000) {
        throw runtime_error("bad telnet command");
    }

    auto l = _querybuf + _qb_pos;
    char* p = FindNL(l, stopPos-_qb_pos);

    while(p!=NULL)
    {
        p[0] = '\0';
        int argc = 0;
        auto args = sdssplitargs(l, &argc);
        if(args!=NULL)
        {
            if(argc>0)
            {
                auto ci = new vector<sds>();
                for(int i = 0; i < argc; i++)
                {
                    ci->push_back(args[i]);
                }
                _cmds.push_back(ci);
                _needRelease.push_back(args);             
            }
            else
            {
                sdsfreesplitres(args, argc);
            }
        }
        else
        {
            auto s = sdsnew("-bad command format\r\n");
            FeedReplyBuf(s);
            sdsfree(s);
        }
        
        _qb_pos += (p-l+2);

        if(_qb_pos == stopPos)
            break;
        l = _querybuf + _qb_pos;
        p = FindNL(l, stopPos-_qb_pos+1);
    }
}

//Find NL
char* Client::FindNL(char* l, int len)
{
    char* p = l;
    int pos = 1;  
    while(pos<len) {
        if(l[pos-1]=='\r' && l[pos]=='\n') {
            return &l[pos-1];
        }
        pos++;
    }
    return NULL;
}


//读取任意一段
bool Client::ReadCmdSegment(int stopPos)
{
    if(_next_len>0)
        return ReadCmdSegmentStr(stopPos) ? (_qb_pos != stopPos) : false;

    char * p = _querybuf + _qb_pos;
    switch (*p)
    {
        case '*':
        {
            if(_next_count>0) {
                throw runtime_error("bad command format1");
            }
            int ii = -1;
            if(ReadCmdSegmentInt(stopPos, &ii)) {
                _next_count = ii;
                return _qb_pos != stopPos;
            }
            return false;
        }

        case '$':
        {
            if(_next_count<0) {
                throw runtime_error("bad command format3");
            }
            
            int ii = -1;
            if(ReadCmdSegmentInt(stopPos, &ii)) {
                _next_len = ii;
                return _qb_pos != stopPos;
            }
            return false;
        }

        case '\n':
        case '\r':
            return ++_qb_pos != stopPos;
    
        default:
            throw runtime_error("bad command format6");
    }
}

//读取数字字段
bool Client::ReadCmdSegmentInt(int stopPos, int* len)
{
    int pos = _qb_pos + 1;
    if(pos + 2 > stopPos)
        return false;
    int reti = 0;
    auto p = _querybuf + pos;

    while(*p!='\r') {
        reti = (reti*10) + (*p-'0');
        //已经到达尾部
        if(++pos == stopPos)
            return false;
        p++;
    }

    if(reti <= 0) {
        throw runtime_error("bad number");
    }

    if(stopPos>pos && stopPos>++pos) {
        pos++;
    }
    _qb_pos = pos;
    *len = reti;
    return true;
}

//读取字符串段
bool Client::ReadCmdSegmentStr(int stopPos)
{
    if(_qb_pos + _next_len > stopPos)   //还不够数
        return false;
    auto s = sdsnew(NULL);
    s = sdscatlen(s, _querybuf+_qb_pos, _next_len);

    if(_next_count<0) {
        throw runtime_error("bad command format7");
    }
    else {
        _args->push_back(s);
        if(_args->size()==_next_count) {
            _cmds.push_back(_args);
            _args = new vector<sds>;
            _next_count = -1;
        }       
    }

    _qb_pos += _next_len;
    _next_len = -1;
    if(stopPos>_qb_pos && stopPos>++_qb_pos) {
        _qb_pos++;
    }
    return true;
}


//追加格式化结果输出
void Client::AppendReplyBuf(const char* fmt, const char* c) {
    _replybuf = sdscatfmt(_replybuf, fmt, c);
}

void Client::AppendReplyBuf(const char* fmt, sds c, size_t i) {
    _replybuf = sdscatfmt(_replybuf, fmt, c, i);
}

void Client::AppendReplyBuf(sds str) {
    _replybuf = sdscatfmt(_replybuf, "$%u\r\n%S\r\n", (unsigned int)sdslen(str), str);
}

void Client::AppendReplyBufArgError() {
    _replybuf = sdscat(_replybuf, "-parameter error\r\n");
}

void Client::AppendReplyBufError(const char* err) {
    _replybuf = sdscatfmt(_replybuf, "-%s\r\n", err);
}

void Client::AppendReplyBufOk() {
    _replybuf = sdscatlen(_replybuf, "+ok\r\n", 5);
}

void Client::AppendReplyBufNotFound() {   
    _replybuf = sdscat(_replybuf, "+not found\r\n");
}

void Client::AppendReplyBuf(sds* args, unsigned int argc) {
    _replybuf = sdscatfmt(_replybuf, "*%u\r\n", argc);
    for(int i = 0; i < argc; i++) {
        _replybuf = sdscatfmt(_replybuf, "$%u\r\n%S\r\n", (unsigned int)sdslen(args[i]), args[i]);
    }
}
