#include <lib_lineinput.h>
#include <lib_printf.h>
#include <lib_string.h>
#include <lib_syscalls.h>
int
_start(int a1,int a2)
{
    int d = msgopen("mirb");
    char buf[100];
    struct msgbuf mbuf;

    msgsendint(d,0);
    printf("---Tiny embeddable interactive ruby shell in BitVisor---\n");
    for(;;){
         printf ("mirb > ");
         lineinput (buf, 100);

         setmsgbuf(&mbuf,buf,sizeof buf,0);
         msgsendbuf(d,1,&mbuf,1);
        if (strcmp (buf, "exit")==0) break;
    }
    msgclose(d);

    printf("\n------\n");
    exitprocess(0);
    return 0;
}
