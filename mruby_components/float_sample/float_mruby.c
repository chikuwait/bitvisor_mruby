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
uint8_t mrb_float_code[];
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
floatmruby_thread(void *arg)
{
    mrb_state *mrb = mrb_open_allocf(allocate,NULL);
    struct RClass *bitvisor;
    if(mrb != NULL){
        bitvisor = mrb_define_class(mrb,"Bitvisor",mrb->object_class);
        mrb_define_class_method(mrb,bitvisor,"print",bitvisor_print,MRB_ARGS_REQ(1));
        mrb_load_irep(mrb,mrb_float_code);
        mrbc_context *cxt = mrbc_context_new(mrb);
        mrb_load_string_cxt(mrb,"",cxt);
        mrbc_context_free(mrb,cxt);
        mrb_close(mrb);
    }
    thread_exit();
}

static void
floatmruby_kernel_init(void)
{
    thread_new(floatmruby_thread, NULL, VMM_STACKSIZE);
}
INITFUNC("vmmcal0", floatmruby_kernel_init);
