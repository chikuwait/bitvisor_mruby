#include <stdio.h>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/value.h>
#include <mruby/string.h>
#include <mruby/irep.h>
#include <mruby/proc.h>
mrb_value
test_print(mrb_state *mrb,mrb_value self)
{
    mrb_value str;
    mrb_get_args(mrb, "S", &str);
    printf("%s", RSTRING_PTR(str));
}
uint8_t mrb_beat_code[];
int
main(void)
{
    mrb_state *mrb = mrb_open();
    if (!mrb) { /*  handle error */ }
    struct RClass *bitvisor;
    bitvisor = mrb_define_class(mrb,"Bitvisor",mrb->object_class);
    mrb_define_class_method(mrb,bitvisor,"print",test_print,MRB_ARGS_REQ(1)); 
    mrb_load_irep(mrb,mrb_beat_code);
    mrbc_context *cxt = mrbc_context_new(mrb);
    mrb_load_string_cxt(mrb,"",cxt);
    mrbc_context_free(mrb,cxt);
    mrb_close(mrb);
    return 0;
}
