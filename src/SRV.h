/*
** srv.h for SRV in /home/pi/srv/src/srv
**
** Made by solidest
** Login   <>
**
** Started on  Fri Nov 15 9:02:15 AM 2019 solidest
** Last update Sat Nov 15 9:06:22 AM 2019 solidest
*/

#ifndef SRV_H_
# define SRV_H_

#include <uv.h>
#include "server/Client.h"


#define CheckArgSize(i) if(ci.size()!=i) { c.AppendReplyBufArgError(); return; }

typedef void (*cmd_fun) (Client& c, vector<sds>& ci);
typedef struct  CommandInfo
{
    const char * name;
    cmd_fun cmd;
    unsigned int allowMode;
} CommandInfo;

//for server
void StartServer(const char* ip, unsigned short port);
void CloseClient(Client* c);
        
//for uv
void OnClientConnect(uv_stream_t *s, int status);
void OnRead(uv_stream_t *c, ssize_t nread, const uv_buf_t *buf);
void OnNewBuffer(uv_handle_t *c, size_t suggested_size, uv_buf_t *buf);
void OnClientClosed(uv_handle_t *c);
void OnAfterSend(uv_write_t* req, int status);
void OnShowDown(uv_shutdown_t *  req, int status);

//for command
void ExecCommand(Client& c, vector<sds>& ci);
void cmdPing(Client& c, vector<sds>& ci);
void cmdState(Client& c, vector<sds>& ci);
void cmdExit(Client& c, vector<sds>& ci);
void cmdNil(Client& c, vector<sds>& ci);
void cmdPrepare(Client& c, vector<sds>& ci);
void cmdStartCase(Client& c, vector<sds>& ci);
void cmdPause(Client& c, vector<sds>& ci);
void cmdContinue(Client& c, vector<sds>& ci);
void cmdStopCase(Client& c, vector<sds>& ci);

//helper
void InitialCmds();
void Reply(uv_stream_t *h, sds buff);

#endif /* !SRV_H_ */
