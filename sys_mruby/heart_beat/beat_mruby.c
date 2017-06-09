#include <core.h>
#include <core/thread.h>
#include <core/time.h>
#include <mruby.h>
#include <mruby/compile.h>

static void
heartbeat_thread(void *arg)
{
    mrb_state *mrb = mrb_open();
    mrb_load_string(mrb,"p 'hello mruby'");
    mrb_close(mrb);

    u64 cur, prev;
    prev = get_time();
    for(;;){
        schedule();
        cur = get_time();

        if(cur - prev >= 5 * 1000 * 1000 ){
            printf("%11lld:tick!\n", cur);
            prev = cur;
        }
    }
    thread_exit();
}

static void 
heartbeat_kernel_init(void)
{    
    thread_new(heartbeat_thread, NULL, VMM_STACKSIZE);
}
INITFUNC("vmmcal0", heartbeat_kernel_init);
