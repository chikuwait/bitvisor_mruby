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
uint8_t mrb_beat_code[];
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

mrb_value
bitvisor_get_time(mrb_state *mrb,mrb_value self)
{
    mrb_int a;
    a = get_time();
    return mrb_fixnum_value(a);
}
mrb_value
bitvisor_set_schedule(mrb_state *mrb,mrb_value self)
{
    schedule();
}
mrb_value mrb_get_backtrace(mrb_state *mrb, mrb_value self);

static void
heartbeat_thread(void *arg)
{
    mrb_state *mrb = mrb_open_allocf(allocate,NULL);

    int ai = mrb_gc_arena_save(mrb);
    struct RClass *bitvisor;
    if(mrb != NULL){
        bitvisor = mrb_define_class(mrb,"Bitvisor",mrb->object_class);
        mrb_define_class_method(mrb,bitvisor,"print",bitvisor_print,ARGS_REQ(1));
        mrb_define_class_method(mrb,bitvisor,"get_time",bitvisor_get_time,ARGS_NONE());
        mrb_define_class_method(mrb,bitvisor,"set_schedule",bitvisor_set_schedule,ARGS_NONE());
        mrb_load_irep(mrb,mrb_beat_code);

        mrbc_context *cxt = mrbc_context_new(mrb);
        mrbc_filename(mrb, cxt, "foo.rb");
        mrb_load_string_cxt(mrb,"",cxt);
/*
 *      mrb_value exc = mrb_obj_value(mrb->exc);
 *      mrb_value backtrace = mrb_get_backtrace(mrb,exc);
 *      printf("%s\n",mrb_str_to_cstr(mrb,mrb_inspect(mrb,backtrace)));
 *      mrb_value inspect = mrb_inspect(mrb,exc);
 *      printf("%s\n",mrb_str_to_cstr(mrb,inspect));
 *      mrb->exc = 0;
 *       mrb_gc_arena_restore(mrb,ai);
        */
        mrbc_context_free(mrb,cxt);
        mrb_close(mrb);
    }
    thread_exit();
}

static void
heartbeat_kernel_init(void)
{
    printf("heartbeat_kernel_init invoked.\n");
    thread_new(heartbeat_thread, NULL, VMM_STACKSIZE);
}
INITFUNC("vmmcal0", heartbeat_kernel_init);
