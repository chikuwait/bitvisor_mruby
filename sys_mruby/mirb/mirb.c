#include <core.h>
#include <stdint.h>
#include <core/printf.h>
#include <core/thread.h>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/value.h>
#include <mruby/string.h>
#include <mruby/irep.h>
#include <lib_syscalls.h>

void
*mirb_allocate(struct mrb_state *mirb, void *p, size_t size, void *ud)
{
    if(size == 0){
        if(p != 0){
            free(p);
        }
        return NULL;
    }else{
        return realloc(p, size);
    }
}
mrb_state *mirb;
mrbc_context *mrb_cxt;
mrb_value mrb_ret_value;
static int
mirb_msghandler(int m, int c, struct msgbuf *buf,int bufcnt)
{
    unsigned char *p ;
    if (m == 1 && bufcnt >= 1) {
        if((strcmp((char *)buf->base,"exit"))==0){
            mrbc_context_free(mirb,mrb_cxt);
            mrb_close(mirb);

            mirb = mrb_open_allocf(mirb_allocate,NULL);
            mrb_cxt = mrbc_context_new(mirb);
            return 0;
        }
        p=buf->base;
        mrb_ret_value = mrb_load_string_cxt(mirb,p,mrb_cxt);
        if(mrb_fixnum_p(mrb_ret_value)){
            int intbuf= mrb_fixnum(mrb_ret_value);
            printf(">> %d\n",intbuf);
        }
        if(mrb_string_p(mrb_ret_value)){
            char *charbuf = RSTRING_PTR(mrb_ret_value);
            printf(">> %s\n",charbuf);
        }
    }
    return 0;
}
void
mirb_init(void)
{
    mirb = mrb_open_allocf(mirb_allocate,NULL);
    mrb_cxt = mrbc_context_new(mirb);
    msgregister("mirb", mirb_msghandler);
}

INITFUNC("msg1",mirb_init);
