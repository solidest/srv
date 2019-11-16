/*
** CTX.h for CVT in /home/pi/cvt/src
**
** Made by solidest
** Login   <solidest>
**
** Started on  undefined Nov 10 7:17:57 PM 2019 solidest
** Last update Sun Nov 16 4:07:02 PM 2019 solidest
*/

#ifndef CTX_H_
# define CTX_H_

#include <vector>
#include <atomic>
#include <uv.h>
#include <hiredis/hiredis.h>
#include "json.hpp"

using namespace nlohmann;
using namespace std;

# define WORK_TYPE_IDLE         1
# define WORK_TYPE_PREPARED     2
# define WORK_TYPE_RUNNING      4
# define WORK_TYPE_PAUSE        8
# define WORK_TYPE_ALL          (WORK_TYPE_IDLE|WORK_TYPE_PREPARED|WORK_TYPE_RUNNING|WORK_TYPE_PAUSE)

//global context
class CTX {

    public:
        static void Initial(int workerThreadCount, uv_loop_t* mainLoop);
        static void Release();
        static void Tick();
        static unsigned long long Now();
        static void LogCVT(const string& name, const json& value);

        static const char* WorkTypeStr();
        static const uint32_t WorkType() { return _work_type; }
        static const int WorkProjectId() { return _proj_id; }
        static atomic_uint32_t _work_type;


    private:
        static redisAsyncContext* _asyncRedis;
        static vector<redisContext*> _redisConns;
        static uv_loop_t* _mainLoop;
        static atomic_ullong _now;
        static int _maxStep;

        //static atomic_uint32_t _work_type;
        static int _proj_id;
};

#endif /* !CTX_H_ */
