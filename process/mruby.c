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
mrb_value mrb_get_backtrace(mrb_state *mrb, mrb_value self);

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

typedef struct{
    mrb_state *mrb;
    mrbc_context *cxt;
    int ai;
}mrb_workspace;
mrb_workspace space;

void
mrb_create_workspace(){
    space.mrb = mrb_open_allocf(allocate,NULL);
    space.cxt = mrbc_context_new(space.mrb);
    struct RClass *bitvisor;
    space.ai = mrb_gc_arena_save(space.mrb);
    if(space.mrb != NULL){
        bitvisor = mrb_define_class(space.mrb,"Bitvisor",space.mrb->object_class);
        mrb_define_class_method(space.mrb,bitvisor,"print",bitvisor_print,MRB_ARGS_REQ(1));
    }
}

void
mrb_exit_workspace(){
    if(space.mrb != NULL){
        mrb_close(space.mrb);
        printf("mruby process was completed successfuly.\n");
    }else{
        printf("Error : mruby process does not already exist\n");
    }
    exitprocess(0);
}

void
mrb_error_handler(){
    mrb_value exc = mrb_obj_value(space.mrb->exc);
    mrb_value inspect = mrb_inspect(space.mrb, exc);
    printf("[%s]\n",mrb_str_to_cstr(space.mrb, inspect));
    mrb_value backtrace = mrb_get_backtrace(space.mrb,exc);
    printf("[%s]\n",mrb_str_to_cstr(space.mrb, mrb_inspect(space.mrb, backtrace)));
    space.mrb->exc = 0;
    mrb_gc_arena_restore(space.mrb, space.ai);
    mrbc_context_free(space.mrb,space.cxt);
    mrb_exit_workspace();

}

void
mrb_load_workspace(){
        mrb_load_irep(space.mrb,mrb_hello_code);
        if(space.mrb->exc != 0){
            mrb_error_handler();
        }
}
void
mrb_callback_reciver(int arg, struct msgbuf *buf){
    unsigned char *funcall = buf[0].base;
    if(arg == 0){
        mrb_funcall(space.mrb,mrb_top_self(space.mrb),funcall,0);
    }else{
        unsigned char *arg = buf[1].base;
        mrb_funcall(space.mrb,mrb_top_self(space.mrb),funcall,1,mrb_str_new_cstr(space.mrb,arg));
    }
}
void
mrb_set_pointer(struct msgbuf *buf){
    u8 *q = buf->base;
    printf("Header:process= %0X:%0X:%0X:%0X:%0X:%0X\n",(u8 *)q[0],(u8 *)q[1],(u8 *)q[2],(u8 *)q[3],(u8 *)q[4],(u8 *)q[5]);
}
int
_start(int a1, int a2,struct msgbuf *buf, int bufcnt)
{
    switch(a2){
        case 0 :
            mrb_create_workspace();
            break;
        case 1 :
            mrb_load_workspace();
            break;
        case 2 :
            mrb_exit_workspace();
            break;
        case 3:
            mrb_callback_reciver(0,buf);
            break;
        case 4:
            mrb_callback_reciver(1,buf);
        case 5:
            mrb_set_pointer(buf);
    }
    return 0;
}
