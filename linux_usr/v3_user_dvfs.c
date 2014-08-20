#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "v3_ctrl.h"
#include "v3_user_dvfs.h"

int v3_user_dvfs_acquire_direct(uint32_t core)
{
    struct v3_dvfs_ctrl_request r;

    r.cmd=V3_DVFS_ACQUIRE;
    r.acq_type=V3_DVFS_DIRECT;
    r.pcore=core;
    r.freq_khz=0;
    r.pstate=0;

    return v3_dev_ioctl(V3_DVFS_CTRL,&r);
}

int v3_user_dvfs_acquire_external(uint32_t core)
{
    struct v3_dvfs_ctrl_request r;

    r.cmd=V3_DVFS_ACQUIRE;
    r.acq_type=V3_DVFS_EXTERNAL;
    r.pcore=core;
    r.freq_khz=0;
    r.pstate=0;

    return v3_dev_ioctl(V3_DVFS_CTRL,&r);
}

int v3_user_dvfs_release(uint32_t core)
{
    struct v3_dvfs_ctrl_request r;

    r.cmd=V3_DVFS_RELEASE;
    r.acq_type=V3_DVFS_DIRECT;
    r.pcore=core;
    r.freq_khz=0;
    r.pstate=0;

    return v3_dev_ioctl(V3_DVFS_CTRL,&r);
}


int v3_user_dvfs_set_pstate(uint32_t core, uint8_t pstate)
{
    struct v3_dvfs_ctrl_request r;

    r.cmd=V3_DVFS_SETPSTATE;
    r.acq_type=V3_DVFS_DIRECT;
    r.pcore=core;
    r.freq_khz=0;
    r.pstate=pstate;

    return v3_dev_ioctl(V3_DVFS_CTRL,&r);
}

int v3_user_dvfs_set_freq(uint32_t core, uint64_t freq_khz)
{
    struct v3_dvfs_ctrl_request r;

    r.cmd=V3_DVFS_SETFREQ;
    r.acq_type=V3_DVFS_EXTERNAL;
    r.pcore=core;
    r.freq_khz=freq_khz;
    r.pstate=0;

    return v3_dev_ioctl(V3_DVFS_CTRL,&r);
}

