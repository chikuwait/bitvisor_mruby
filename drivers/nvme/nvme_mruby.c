#include <core.h>
#include <storage.h>

#include "nvme_io.h"

struct nvme_mruby_meta{
	struct nvme_host *host;
	struct storage_device **devices;
	uint id;
	uint n_intercepted_reqs;
};

#define FETCHING_THRESHOLD (32)
static uint mruby_storage_id = 0;

static int
nvme_mruby_interceptor_init(void *interceptor)
{
    //TODO: init
    return NVME_IO_RESUME_FETCHING_GUEST_CMDS;
}

static void
nvme_mruby_intercept_compare(void *interceptor, struct nvme_request *g_req, u32 nsid, u64 start_lba, u16 n_lbas)
{
    panic("nvme_mruby currently does not support Compare commnad");
}

static void
nvme_mruby_filter_identify_data(void *interceptor, u32 nsid, u16 controller_id, u8 cns, u8 *data)
{
    switch (cns)
    {
        case 0x1:
            printf ("Filtering controller %u data\n", controller_id);
            //TODO: controller data filter
            break;

        default:
        printf ("Unknown identify cns: %u, ignore\n", cns);
    }
}
static uint
nvme_mruby_get_fetching_limit (void *interceptor,
		    uint n_waiting_g_req)
{
	struct nvme_mruby_meta *mruby_meta = interceptor;

	uint n_intercepted_reqs = mruby_meta->n_intercepted_reqs;

	if (n_intercepted_reqs > FETCHING_THRESHOLD)
		return 0;

	return FETCHING_THRESHOLD - n_intercepted_reqs;
}

static void
nvme_mruby_intercept_read(void *interceptor, struct nvme_request *g_req, u32 nsid, u64 start_lda, u16 n_lbas)
{
    //TODO: hook read data
    printf("read test\n");
}

static void
nvme_mruby_intercept_write(void *interceptor, struct nvme_request *g_req, u32 nsid, u64 start_lda, u16 n_lbas)
{
    //TODO: hook write data
    printf("write test\n");
}

static int
install_nvme_mruby(struct nvme_host *host)
{
    struct nvme_mruby_meta *mruby_meta = alloc(sizeof ((*mruby_meta)));
    mruby_meta->host = host;
    mruby_meta->devices= NULL;
    mruby_meta->id = mruby_storage_id++;
    mruby_meta->n_intercepted_reqs = 0;

    struct nvme_io_interceptor *io_interceptor;
    io_interceptor = alloc( sizeof(*io_interceptor));
    memset(io_interceptor, 0, sizeof (*io_interceptor));

    io_interceptor->interceptor = mruby_meta;
    io_interceptor->on_init = nvme_mruby_interceptor_init;
    io_interceptor->on_read = nvme_mruby_intercept_read;
    io_interceptor->on_write= nvme_mruby_intercept_write;
    io_interceptor->on_compare= nvme_mruby_intercept_compare;
    io_interceptor->filter_identify_data = nvme_mruby_filter_identify_data;
    io_interceptor->get_fetching_limit = nvme_mruby_get_fetching_limit;
    io_interceptor->serialize_queue_fetch = 1;

    printf("[mruby] Installing mruby hook interceptor\n");
    return nvme_io_install_interceptor (host, io_interceptor);
}

static void
nvme_mruby_ext(void)
{
    nvme_io_register_ext("mruby", install_nvme_mruby);
}
INITFUNC("driver1", nvme_mruby_ext);
