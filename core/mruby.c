#include <core/process.h>
#include <core/thread.h>
#include <core/stdarg.h>

int
create_mruby_process(){
    int ttyin, ttyout,mruby_process;
    ttyin = msgopen("ttyin");
    ttyout = msgopen("ttyout");
    mruby_process = newprocess("mruby");
    if(ttyin <0 || ttyout <0 || mruby_process <0){
        printf("mruby process generate fail. ttyin = %d,ttyout = %d, mruby_process = %d.\n",ttyin,ttyout,mruby_process);
        return -1;
    }
    msgsenddesc(mruby_process,ttyin);
    msgsenddesc(mruby_process,ttyout);
    msgsendint(mruby_process,0);

    return mruby_process;
}

int
load_mruby_process(int mruby_process)
{
    msgsendint(mruby_process,1);
    return 0;
}

int
mruby_funcall(int mruby_process, char *str, int argc, ...){
    va_list ap;
    va_start(ap, argc);
    struct msgbuf mbuf[argc + 1];
    setmsgbuf(&mbuf[0], str, sizeof str, 0);
    for(int i = 0; i < argc ; i++){
        char *arg = va_arg(ap, char *);
        setmsgbuf(&mbuf[i + 1], arg, sizeof arg, i + 1);
    }
    va_end(ap);
    msgsendbuf(mruby_process, argc+3, mbuf, argc + 1);
}

int
mruby_set_pointer(int mruby_process, u8 *p, int byte){
    struct msgbuf mbuf[2];
    struct mempool *mp;
    u8 *send_memp;
    mp = mempool_new(0, 1, true);
    send_memp = mempool_allocmem(mp, sizeof p);
    memcpy(send_memp,p,sizeof p);
    setmsgbuf(&mbuf[0], send_memp, sizeof send_memp, 0);
    setmsgbuf(&mbuf[1], &byte, sizeof byte, 1);
    msgsendbuf(mruby_process,100, mbuf,2);
    mempool_freemem (mp, send_memp);
}

int
exit_mruby_process(int mruby_process)
{
    msgsendint(mruby_process,2);
    return 0;
}

