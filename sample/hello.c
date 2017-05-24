#include "mruby.h"
#include "mruby/compile.h"

int
main(void)
{
    mrb_state *mrb = mrb_open();
    if(!mrb){}
    mrb_load_string(mrb,"p 'hello!'");
    mrb_close(mrb);
    return 0;
}
