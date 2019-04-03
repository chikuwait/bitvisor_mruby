#include <core.h>
#include <storage.h>

#include "nvme_io.h"

static int
install_nvme_mruby(struct nvme_host *host)
{
     struct nvme_io_interceptor *io_interceptor;
     io_interceptor = alloc( sizeof(*io_interceptor));
     memset(io_interceptor, 0, sizeof (*io_interceptor));

    return nvme_io_install_interceptor (host, io_interceptor);
}

static void
nvme_mruby_ext(void)
{
    nvme_io_register_ext("mruby", install_nvme_mruby);
}
INITFUNC("driver1", nvme_mruby_ext);
