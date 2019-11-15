/*
** Client.h for STBox 连入box的客户端
**
** Made by header_template
** Login   <solidest>
**
** Started on  Wed Mar 27 5:29:02 PM 2019 header_template
** Last update Sat Nov 15 9:07:15 AM 2019 solidest
*/

#ifndef CLIENT_H_
# define CLIENT_H_

#include "../sds.h"
#include <deque>
#include <vector>

using namespace std;

# define REQUEST_TYPE_UNKNOW    0
# define REQUEST_TYPE_TELNET    1
# define REQUEST_TYPE_CLIENT    2

class Client
{
private:
    uint64_t _id;                    /* Client incremental unique ID. */
    int _reqtype;                    /* Request protocol type: REQUEST_TYPE_* */

    /* query */
    sds _querybuf;                   /* Buffer we use to accumulate client queries. */
    size_t _qb_pos;                  /* The position we have read in querybuf. */

    /* query parser */
    int _next_len;                   /* Next arg len */
    int _next_count;                 /* Next arg count */               

    /* Current command */
    vector<sds>* _args;
    deque<vector<sds>*> _cmds;

    /* for telnet */
    deque<sds*> _needRelease;

    /* Response buffer */
    sds _replybuf;
    
private:
    static int _count;
    static uint64_t _nextid;

public:
    void* Handle;
    Client(void* handle);
    ~Client();
    void FeedRecvBuf(sds buff);
    void FeedReplyBuf(sds buff);
    void AppendReplyBuf(const char* fmt, const char* c);
    void AppendReplyBuf(const char* fmt, sds c, size_t i);
    void AppendReplyBuf(sds* args, unsigned int argc);
    void AppendReplyBuf(sds str);
    void AppendReplyBufArgError();
    void AppendReplyBufNotFound();
    void AppendReplyBufOk();
    void AppendReplyBufError(const char* err);
    sds GetLastErr();
    vector<sds>* NextCmdInfo();
    void PopOneCmdInfo();
    sds FlushReplyBuf();
    size_t ReplySize();

private:
    void ReleaseCmdInfo(vector<sds>* ci);
    void ReadTelnetCommand(int stopPos);
    void ReadCommand();
    bool ReadCmdSegment(int stopPos);
    bool ReadCmdSegmentStr(int stopPos);
    bool ReadCmdSegmentInt(int stopPos, int* len);
    char* FindNL(char* l, int len);
};


#endif /* !CLIENT_H_ */
