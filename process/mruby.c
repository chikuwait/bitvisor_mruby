#include <lib_printf.h>
#include <lib_mm.h>
#include <lib_stdlib.h>
#include <lib_syscalls.h>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/value.h>
#include <mruby/string.h>
#include <mruby/irep.h>
#include <mruby/proc.h>
uint8_t mrb_hello_code[];
int heap[1048576], heaplen = 1048576;
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
mrb_value mrb_get_backtrace(mrb_state *mrb, mrb_value self);

int
_start(int a1,int a2)
{

    printf("mruby process registered\n");
    mrb_state *mrb = mrb_open_allocf(allocate,NULL);
    printf("mruby open\n");
    struct RClass *bitvisor;
    int ai = mrb_gc_arena_save(mrb);

    if(mrb != NULL){
        bitvisor = mrb_define_class(mrb,"Bitvisor",mrb->object_class);
        mrb_define_class_method(mrb,bitvisor,"print",bitvisor_print,MRB_ARGS_REQ(1));
        mrb_load_irep(mrb,mrb_hello_code);

        mrb_value exc = mrb_obj_value(mrb->exc);
        mrb_value backtrace = mrb_get_backtrace(mrb,exc);
        printf("%s\n",mrb_str_to_cstr(mrb, mrb_inspect(mrb, backtrace)));
        mrb_value inspect = mrb_inspect(mrb, exc);
        printf("%s\n",mrb_str_to_cstr(mrb, inspect));

        // 例外をクリア
        mrb->exc = 0;
        mrb_gc_arena_restore(mrb, ai);

        mrbc_context *cxt = mrbc_context_new(mrb);
        mrb_load_string_cxt(mrb,"",cxt);
        mrbc_context_free(mrb,cxt);
        mrb_close(mrb);
    }
    return 0;
}
