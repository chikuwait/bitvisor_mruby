#include <core/process.h>
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
exit_mruby_process(int mruby_process)
{
    msgsendint(mruby_process,2);
    return 0;
}


