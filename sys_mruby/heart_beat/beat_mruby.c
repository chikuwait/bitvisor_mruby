#include <core.h>
#include <stdint.h>
#include <core/printf.h>
#include <core/thread.h>
#include <core/time.h>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/value.h>
#include <mruby/string.h>
#include <mruby/irep.h>
#include <mruby/proc.h>
uint8_t chikuwait[];
void
*allocate(struct mrb_state *mrb, void *p, size_t size, void *ud)
{
    if (size == 0){
        if(p != 0 ){
            free(p);
        }
        return NULL;
    }else {
        return realloc(p, size);
    }
}
mrb_value
bitvisor_print(mrb_state *mrb,mrb_value self)
{
    mrb_value str;
    mrb_get_args(mrb, "S", &str);
    printf("%s", RSTRING_PTR(str));
}
static void
heartbeat_thread(void *arg)
{
    mrb_state *mrb = mrb_open_allocf(allocate,NULL);
    struct RClass *bitvisor;
    if(mrb != NULL){
        bitvisor = mrb_define_class(mrb,"Bitvisor",mrb->object_class);
        mrb_define_class_method(mrb,bitvisor,"print",bitvisor_print,ARGS_REQ(1));
        mrb_load_irep(mrb,chikuwait);
        mrb_close(mrb);
/*
        mrbc_context *cxt = mrbc_context_new(mrb);
        mrb_load_string_cxt(mrb,"def test(num1,num2);return ([num1..num2].inject{|sum, n| sum + n});",cxt);

        mrb_value test = mrb_funcall(mrb,mrb_top_self(mrb),"test",2,mrb_fixnum_value(1),mrb_fixnum_value(10));
        int ret = mrb_fixnum(test);

        printf("ret = %d\n",ret);
        mrbc_context_free(mrb,cxt);
        mrb_close(mrb);*/
    }

    /*  u64 cur, prev;
    prev = get_time();
    for(;;){
        schedule();
        cur = get_time();

        if(cur - prev >= 5 * 1000 * 1000 ){
            printf("%11lld:tick!\n", cur);
            prev = cur;
        }
    }*/
    thread_exit();
}

static void
heartbeat_kernel_init(void)
{
    printf("heartbeat_kernel_init invoked.\n");
    //volatile mrb_state *mrb = mrb_open();
    //printf("heartbeat_kernel_init: mrb_open() = %p .\n", mrb);
    thread_new(heartbeat_thread, NULL, VMM_STACKSIZE);
}
INITFUNC("vmmcal0", heartbeat_kernel_init);
