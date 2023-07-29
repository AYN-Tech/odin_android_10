#!/vendor/bin/sh

#Copyright (c) 2019, The Linux Foundation. All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
#      copyright notice, this list of conditions and the following
#      disclaimer in the documentation and/or other materials provided
#      with the distribution.
#    * Neither the name of The Linux Foundation nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
#WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
#ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
#BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
#BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
#OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
#IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

enable_atoll_tracing_events()
{
    # timer
    echo 1 > /sys/kernel/debug/tracing/events/timer/timer_expire_entry/enable
    echo 1 > /sys/kernel/debug/tracing/events/timer/timer_expire_exit/enable
    #echo 1 > /sys/kernel/debug/tracing/events/timer/hrtimer_cancel/enable
    echo 1 > /sys/kernel/debug/tracing/events/timer/hrtimer_expire_entry/enable
    echo 1 > /sys/kernel/debug/tracing/events/timer/hrtimer_expire_exit/enable
    #echo 1 > /sys/kernel/debug/tracing/events/timer/hrtimer_init/enable
    #echo 1 > /sys/kernel/debug/tracing/events/timer/hrtimer_start/enable
    #enble FTRACE for softirq events
    echo 1 > /sys/kernel/debug/tracing/events/irq/enable
    #enble FTRACE for Workqueue events
    echo 1 > /sys/kernel/debug/tracing/events/workqueue/enable
    # schedular
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_cpu_hotplug/enable
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_migrate_task/enable
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_pi_setprio/enable
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_switch/enable
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_wakeup/enable
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_wakeup_new/enable
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_isolate/enable
    # sound
    echo 1 > /sys/kernel/debug/tracing/events/asoc/snd_soc_reg_read/enable
    echo 1 > /sys/kernel/debug/tracing/events/asoc/snd_soc_reg_write/enable
    # mdp
    echo 1 > /sys/kernel/debug/tracing/events/mdss/mdp_video_underrun_done/enable
    # video
    echo 1 > /sys/kernel/debug/tracing/events/msm_vidc/enable
    # clock
    echo 1 > /sys/kernel/debug/tracing/events/power/clock_set_rate/enable
    echo 1 > /sys/kernel/debug/tracing/events/power/clock_enable/enable
    echo 1 > /sys/kernel/debug/tracing/events/power/clock_disable/enable
    echo 1 > /sys/kernel/debug/tracing/events/power/cpu_frequency/enable

    # power
    echo 1 > /sys/kernel/debug/tracing/events/msm_low_power/cpu_idle_enter/enable
    echo 1 > /sys/kernel/debug/tracing/events/msm_low_power/cpu_idle_exit/enable
    #thermal
    echo 1 > /sys/kernel/debug/tracing/events/thermal/thermal_zone_trip/enable
    echo 1 > /sys/kernel/debug/tracing/events/thermal/thermal_temperature/enable
    echo 1 > /sys/kernel/debug/tracing/events/thermal/thermal_set_trip/enable
    echo 1 > /sys/kernel/debug/tracing/events/thermal/cdev_update_start/enable
    echo 1 > /sys/kernel/debug/tracing/events/thermal/cdev_update/enable

    #rmph_send_msg
    echo 1 > /sys/kernel/debug/tracing/events/rpmh/rpmh_send_msg/enable
    #SCM Tracing enabling
    echo 1 > /sys/kernel/debug/tracing/events/scm/enable

    #Preempt trace events
    echo 'nsec' > /proc/sys/kernel/preemptoff_tracing_threshold_ns
    echo 400000000 > /proc/sys/kernel/preemptoff_tracing_threshold_ns
    echo 1 > /sys/kernel/debug/tracing/events/sched/sched_preempt_disable/enable
    #irqsoff trace events
    echo 'nsec' > /proc/sys/kernel/irqsoff_tracing_threshold_ns
    echo 100000000 > /proc/sys/kernel/irqsoff_tracing_threshold_ns
    echo 1 > /sys/kernel/debug/tracing/events/preemptirq/irqs_disable/enable

    echo 1 > /sys/kernel/debug/tracing/tracing_on
}

# function to enable ftrace events
enable_atoll_ftrace_event_tracing()
{
    # bail out if its perf config
    if [ ! -d /sys/module/msm_rtb ]
    then
        return
    fi

    # bail out if ftrace events aren't present
    if [ ! -d /sys/kernel/debug/tracing/events ]
    then
        return
    fi

    enable_atoll_tracing_events
}

# function to enable ftrace event transfer to CoreSight STM
enable_atoll_stm_events()
{
    # bail out if its perf config
    if [ ! -d /sys/module/msm_rtb ]
    then
        return
    fi
    # bail out if coresight isn't present
    if [ ! -d /sys/bus/coresight ]
    then
        return
    fi
    # bail out if ftrace events aren't present
    if [ ! -d /sys/kernel/debug/tracing/events ]
    then
        return
    fi
    echo 0x4096 > /sys/kernel/debug/tracing/buffer_size_kb
    echo $etr_size > /sys/bus/coresight/devices/coresight-tmc-etr/mem_size
    echo sg > /sys/bus/coresight/devices/coresight-tmc-etr/mem_type
    echo 1 > /sys/bus/coresight/devices/coresight-tmc-etr/$sinkenable
    echo 1 > /sys/bus/coresight/devices/coresight-stm/$srcenable
    echo 1 > /sys/kernel/debug/tracing/tracing_on
    echo 0 > /sys/bus/coresight/devices/coresight-stm/hwevent_enable
    enable_atoll_tracing_events
}

config_atoll_dcc_gemnoc()
{
    #Gladiator
    echo 0x9680000 > $DCC_PATH/config
    echo 0x9680004 > $DCC_PATH/config
    echo 8 > $DCC_PATH/loop
    echo 0x9681000 > $DCC_PATH/config
    echo 1 > $DCC_PATH/loop
    echo 0x9681004 > $DCC_PATH/config
    echo 0x9681008 > $DCC_PATH/config
    echo 0x968100c > $DCC_PATH/config
    echo 0x9681010 > $DCC_PATH/config
    echo 0x9681014 > $DCC_PATH/config
    echo 0x968101c > $DCC_PATH/config
    echo 0x9681020 > $DCC_PATH/config
    echo 0x9681024 > $DCC_PATH/config
    echo 0x9681028 > $DCC_PATH/config
    echo 0x968102c > $DCC_PATH/config
    echo 0x9681030 > $DCC_PATH/config
    echo 0x9681034 > $DCC_PATH/config
    echo 0x968103c > $DCC_PATH/config

    echo 0x9698100 > $DCC_PATH/config
    echo 0x9698104 > $DCC_PATH/config
    echo 0x9698108 > $DCC_PATH/config
    echo 0x9698110 > $DCC_PATH/config
    echo 0x9698120 > $DCC_PATH/config
    echo 0x9698124 > $DCC_PATH/config
    echo 0x9698128 > $DCC_PATH/config
    echo 0x969812c > $DCC_PATH/config
    echo 0x9698130 > $DCC_PATH/config
    echo 0x9698134 > $DCC_PATH/config
    echo 0x9698138 > $DCC_PATH/config
    echo 0x969813c > $DCC_PATH/config
    #GEMNOC Registers
    echo 0x9698500 > $DCC_PATH/config
    echo 0x9698504 > $DCC_PATH/config
    echo 0x9698508 > $DCC_PATH/config
    echo 0x969850c > $DCC_PATH/config
    echo 0x9698510 > $DCC_PATH/config
    echo 0x9698514 > $DCC_PATH/config
    echo 0x9698518 > $DCC_PATH/config
    echo 0x969851c > $DCC_PATH/config

    echo 0x9698700 > $DCC_PATH/config
    echo 0x9698704 > $DCC_PATH/config
    echo 0x9698708 > $DCC_PATH/config
    echo 0x969870c > $DCC_PATH/config
    echo 0x9698714 > $DCC_PATH/config
    echo 0x9698718 > $DCC_PATH/config
    echo 0x969871c > $DCC_PATH/config
}

config_atoll_dcc_noc_err_regs()
{
    #CNOC
    # echo 0x1500204 > $DCC_PATH/config
    # echo 0x1500240 > $DCC_PATH/config
    # echo 0x1500244 > $DCC_PATH/config
    # echo 0x1500248 > $DCC_PATH/config
    # echo 0x150024C > $DCC_PATH/config
    # echo 0x1500250 > $DCC_PATH/config
    # echo 0x1500258 > $DCC_PATH/config
    # echo 0x1500288 > $DCC_PATH/config
    # echo 0x150028C > $DCC_PATH/config
    # echo 0x1500290 > $DCC_PATH/config
    # echo 0x1500294 > $DCC_PATH/config
    # echo 0x15002A8 > $DCC_PATH/config
    # echo 0x15002AC > $DCC_PATH/config
    # echo 0x15002B0 > $DCC_PATH/config
    # echo 0x15002B4 > $DCC_PATH/config
    # echo 0x1500300 > $DCC_PATH/config
    # echo 0x1500304 > $DCC_PATH/config
    # echo 0x1500010 > $DCC_PATH/config
    # echo 0x1500020 > $DCC_PATH/config
    # echo 0x1500024 > $DCC_PATH/config
    # echo 0x1500028 > $DCC_PATH/config
    # echo 0x150002C > $DCC_PATH/config
    # echo 0x1500030 > $DCC_PATH/config
    # echo 0x1500034 > $DCC_PATH/config
    # echo 0x1500038 > $DCC_PATH/config
    # echo 0x150003C > $DCC_PATH/config
    #SNOC
    echo 0x1620204 > $DCC_PATH/config
    echo 0x1620240 > $DCC_PATH/config
    echo 0x1620248 > $DCC_PATH/config
    echo 0x1620288 > $DCC_PATH/config
    echo 0x162028C > $DCC_PATH/config
    echo 0x1620290 > $DCC_PATH/config
    echo 0x1620294 > $DCC_PATH/config
    echo 0x16202A8 > $DCC_PATH/config
    echo 0x16202AC > $DCC_PATH/config
    echo 0x16202B0 > $DCC_PATH/config
    echo 0x16202B4 > $DCC_PATH/config
    echo 0x1620300 > $DCC_PATH/config
    #AGGNOC
    echo 0x16E0404 > $DCC_PATH/config
    echo 0x16E0408 > $DCC_PATH/config
    echo 0x16E0410 > $DCC_PATH/config
    echo 0x16E0420 > $DCC_PATH/config
    echo 0x16E0424 > $DCC_PATH/config
    echo 0x16E0428 > $DCC_PATH/config
    echo 0x16E042C > $DCC_PATH/config
    echo 0x16E0430 > $DCC_PATH/config
    echo 0x16E0434 > $DCC_PATH/config
    echo 0x16E0438 > $DCC_PATH/config
    echo 0x16E043C > $DCC_PATH/config
    echo 0x16E0300 > $DCC_PATH/config
    echo 0x16E0304 > $DCC_PATH/config
    echo 0x16E0700 > $DCC_PATH/config
    echo 0x16E0704 > $DCC_PATH/config
    echo 0x1700C00 > $DCC_PATH/config
    echo 0x1700C08 > $DCC_PATH/config
    echo 0x1700C10 > $DCC_PATH/config
    echo 0x1700C20 > $DCC_PATH/config
    echo 0x1700C24 > $DCC_PATH/config
    echo 0x1700C28 > $DCC_PATH/config
    echo 0x1700C2C > $DCC_PATH/config
    echo 0x1700C30 > $DCC_PATH/config
    echo 0x1700C34 > $DCC_PATH/config
    echo 0x1700C38 > $DCC_PATH/config
    echo 0x1700C3C > $DCC_PATH/config
    echo 0x1700300 > $DCC_PATH/config
    echo 0x1700304 > $DCC_PATH/config
    echo 0x1700308 > $DCC_PATH/config
    echo 0x170030C > $DCC_PATH/config
    echo 0x1700310 > $DCC_PATH/config
    # echo 0x1700500 > $DCC_PATH/config
    # echo 0x1700504 > $DCC_PATH/config
    # echo 0x1700508 > $DCC_PATH/config
    # echo 0x170050C > $DCC_PATH/config
    echo 0x1700900 > $DCC_PATH/config
    echo 0x1700904 > $DCC_PATH/config
    echo 0x1700908 > $DCC_PATH/config
    #MNOC
    echo 0x1740004 > $DCC_PATH/config
    echo 0x1740008 > $DCC_PATH/config
    echo 0x1740010 > $DCC_PATH/config
    echo 0x1740020 > $DCC_PATH/config
    echo 0x1740024 > $DCC_PATH/config
    echo 0x1740028 > $DCC_PATH/config
    echo 0x174002C > $DCC_PATH/config
    echo 0x1740030 > $DCC_PATH/config
    echo 0x1740034 > $DCC_PATH/config
    echo 0x1740038 > $DCC_PATH/config
    echo 0x174003C > $DCC_PATH/config
    echo 0x1740300 > $DCC_PATH/config
    echo 0x1740304 > $DCC_PATH/config
    echo 0x1740308 > $DCC_PATH/config
    echo 0x174030C > $DCC_PATH/config
    echo 0x1740310 > $DCC_PATH/config
    echo 0x1740314 > $DCC_PATH/config
    #GEM_NOC
    echo 0x9698204 > $DCC_PATH/config
    echo 0x9698240 > $DCC_PATH/config
    echo 0x9698244 > $DCC_PATH/config
    echo 0x9698248 > $DCC_PATH/config
    echo 0x969824C > $DCC_PATH/config
    echo 0x9681010 > $DCC_PATH/config
    echo 0x9681014 > $DCC_PATH/config
    echo 0x9681018 > $DCC_PATH/config
    echo 0x968101C > $DCC_PATH/config
    echo 0x9681020 > $DCC_PATH/config
    echo 0x9681024 > $DCC_PATH/config
    echo 0x9681028 > $DCC_PATH/config
    echo 0x968102C > $DCC_PATH/config
    echo 0x9681030 > $DCC_PATH/config
    echo 0x9681034 > $DCC_PATH/config
    echo 0x968103C > $DCC_PATH/config
    echo 0x9698100 > $DCC_PATH/config
    echo 0x9698104 > $DCC_PATH/config
    echo 0x9698108 > $DCC_PATH/config
    echo 0x9698110 > $DCC_PATH/config
    echo 0x9698120 > $DCC_PATH/config
    echo 0x9698124 > $DCC_PATH/config
    echo 0x9698128 > $DCC_PATH/config
    echo 0x969812C > $DCC_PATH/config
    echo 0x9698130 > $DCC_PATH/config
    echo 0x9698134 > $DCC_PATH/config
    echo 0x9698138 > $DCC_PATH/config
    echo 0x969813C > $DCC_PATH/config
    #DC_NOC
    echo 0x9160204 > $DCC_PATH/config
    echo 0x9160240 > $DCC_PATH/config
    echo 0x9160248 > $DCC_PATH/config
    echo 0x9160288 > $DCC_PATH/config
    echo 0x9160290 > $DCC_PATH/config
    echo 0x9160300 > $DCC_PATH/config
    echo 0x9160304 > $DCC_PATH/config
    echo 0x9160308 > $DCC_PATH/config
    echo 0x916030C > $DCC_PATH/config
    echo 0x9160310 > $DCC_PATH/config
    echo 0x9160314 > $DCC_PATH/config
    echo 0x9160318 > $DCC_PATH/config
    echo 0x9160008 > $DCC_PATH/config
    echo 0x9160010 > $DCC_PATH/config
    echo 0x9160020 > $DCC_PATH/config
    echo 0x9160024 > $DCC_PATH/config
    echo 0x9160028 > $DCC_PATH/config
    echo 0x916002C > $DCC_PATH/config
    echo 0x9160030 > $DCC_PATH/config
    echo 0x9160034 > $DCC_PATH/config
    echo 0x9160038 > $DCC_PATH/config
    echo 0x916003C > $DCC_PATH/config
    #LPASS AGGNOC
    echo 0x63042680 > $DCC_PATH/config
    echo 0x63042684 > $DCC_PATH/config
    echo 0x63042688 > $DCC_PATH/config
    echo 0x63042690 > $DCC_PATH/config
    echo 0x630426A0 > $DCC_PATH/config
    echo 0x630426A4 > $DCC_PATH/config
    echo 0x630426A8 > $DCC_PATH/config
    echo 0x630426AC > $DCC_PATH/config
    echo 0x630426B0 > $DCC_PATH/config
    echo 0x630426B4 > $DCC_PATH/config
    echo 0x630426B8 > $DCC_PATH/config
    echo 0x630426BC > $DCC_PATH/config
    echo 0x63041900 > $DCC_PATH/config
    echo 0x63041D00 > $DCC_PATH/config

    #NOC sensin Registers
    #SNOC_CENTER_NIU_STATUS_SBM0_SENSEIN
    echo 0x1620500 4 > $DCC_PATH/config
    #SNOC_CENTER_NIU_STATUS_SBM1_SENSEIN
    echo 0x1620700 4 > $DCC_PATH/config
    #SNOC_CENTER_SBM_SENSEIN
    echo 0x1620300 > $DCC_PATH/config
    #SNOC_MONAQ_NIU_STATUS_SBM_SENSEIN
    echo 0x1620F00 2 > $DCC_PATH/config
    #SNOC_WEST_NIU_STATUS_SBM_SENSEIN
    echo 0x1620B00 2 > $DCC_PATH/config
    #A1NOC_MONAQ_SBM_SENSEIN
    # echo 0x1700B00 2 > $DCC_PATH/config
    #A1NOC_WEST_SBM_SENSEIN
    # echo 0x1700700 3 > $DCC_PATH/config
    #CNOC_CENTER_STATUS_SBM_SENSEIN
    # echo 0x1500500 7 > $DCC_PATH/config
    #CNOC_MMNOC_STATUS_SBM_SENSEIN
    # echo 0x1500D00 4 > $DCC_PATH/config
    #CNOC_WEST_STATUS_SBM_SENSEIN
    # echo 0x1501100 4 > $DCC_PATH/config
    #DC_NOC_DISABLE_SBM_SENSEIN
    echo 0x9163100 > $DCC_PATH/config
    #GEM_NOC_SBM_MDSP_SAFE_SHAPING_SENSEIN
    echo 0x96AA100 > $DCC_PATH/config
    #LPASS_AG_NOC_SIDEBANDMANAGERSTATUS_MAIN_SIDEBANDMANAGER_SENSEIN
    echo 0x63041D00 > $DCC_PATH/config
    #NPU_SB_SENSEIN
    echo 0x9991500 8 > $DCC_PATH/config
    #GEMNOC_HM_GEM_NOC_SBM_SAFE_SHAPING_SENSEIN
    echo 0x96B8100  > $DCC_PATH/config
    #AGGRE_NOC_CDSP_NOC_SENSEIN
    echo 0x1700500 4 > $DCC_PATH/config
    #VAPSS_SB_SENSEIN
    echo 0x82D1500 5 > $DCC_PATH/config
    #GCC_CDSP_NOC_TBU_GDS_HW_CTRL_IRQ_STATUS
    echo 0x145384 > $DCC_PATH/config
    #GCC_TURING_TBU_CBCR
    echo 0x145000 > $DCC_PATH/config
    #GCC_SYS_NOC_SF_TCU_CBCR
    echo 0x183004 > $DCC_PATH/config
    #GCC_MMU_TCU_CBCR
    echo 0x183008 > $DCC_PATH/config
    #GCC_NPU_CFG_AHB_CBCR
    echo 0x14D004 > $DCC_PATH/config
    #GCC_AGGRE_NOC_AHB_CBCR
    echo 0x182008 > $DCC_PATH/config
}

config_atoll_dcc_shrm()
{
    #SHRM

    #SHRM CSR
    echo 0x9050008 > $DCC_PATH/config
    #echo 0x9050068 > $DCC_PATH/config
    echo 0x9050078 > $DCC_PATH/config


}

config_atoll_dcc_cprh()
{
    #CPRh
}

config_atoll_dcc_rscc_apps()
{
    #RSC
    echo 0x18200400 > $DCC_PATH/config
    echo 0x18200404 > $DCC_PATH/config
    echo 0x18200408 > $DCC_PATH/config
    echo 0x18200038 > $DCC_PATH/config
    echo 0x18200040 > $DCC_PATH/config
    echo 0x18200048 > $DCC_PATH/config
    echo 0x18220038 > $DCC_PATH/config
    echo 0x18220040 > $DCC_PATH/config
    echo 0x182200D0 > $DCC_PATH/config
    echo 0x18200030 > $DCC_PATH/config
    echo 0x18200010 > $DCC_PATH/config

    #RSC-TCS
    echo 0x1822000c > $DCC_PATH/config
    echo 0x18220d14 > $DCC_PATH/config
    echo 0x18220fb4 > $DCC_PATH/config
    echo 0x18221254 > $DCC_PATH/config
    echo 0x182214f4 > $DCC_PATH/config
    echo 0x18221794 > $DCC_PATH/config
    echo 0x18221a34 > $DCC_PATH/config
    echo 0x18221cd4 > $DCC_PATH/config
    echo 0x18221f74 > $DCC_PATH/config
    echo 0x18220d18 > $DCC_PATH/config
    echo 0x18220fb8 > $DCC_PATH/config
    echo 0x18221258 > $DCC_PATH/config
    echo 0x182214f8 > $DCC_PATH/config
    echo 0x18221798 > $DCC_PATH/config
    echo 0x18221a38 > $DCC_PATH/config
    echo 0x18221cd8 > $DCC_PATH/config
    echo 0x18221f78 > $DCC_PATH/config
    echo 0x18220d00 > $DCC_PATH/config
    echo 0x18220d04 > $DCC_PATH/config
    echo 0x18220d1c > $DCC_PATH/config
    echo 0x18220fbc > $DCC_PATH/config
    echo 0x1822125c > $DCC_PATH/config
    echo 0x182214fc > $DCC_PATH/config
    echo 0x1822179c > $DCC_PATH/config
    echo 0x18221a3c > $DCC_PATH/config
    echo 0x18221cdc > $DCC_PATH/config
    echo 0x18221f7c > $DCC_PATH/config
    echo 0x18221274 > $DCC_PATH/config
    echo 0x18221288 > $DCC_PATH/config
    echo 0x1822129c > $DCC_PATH/config
    echo 0x182212b0 > $DCC_PATH/config
    echo 0x182212c4 > $DCC_PATH/config
    echo 0x182212d8 > $DCC_PATH/config
    echo 0x182212ec > $DCC_PATH/config
    echo 0x18221300 > $DCC_PATH/config
    echo 0x18221314 > $DCC_PATH/config
    echo 0x18221328 > $DCC_PATH/config
    echo 0x1822133c > $DCC_PATH/config
    echo 0x18221350 > $DCC_PATH/config
    echo 0x18221364 > $DCC_PATH/config
    echo 0x18221378 > $DCC_PATH/config
    echo 0x1822138c > $DCC_PATH/config
    echo 0x182213a0 > $DCC_PATH/config
    echo 0x18221514 > $DCC_PATH/config
    echo 0x18221528 > $DCC_PATH/config
    echo 0x1822153c > $DCC_PATH/config
    echo 0x18221550 > $DCC_PATH/config
    echo 0x18221564 > $DCC_PATH/config
    echo 0x18221578 > $DCC_PATH/config
    echo 0x1822158c > $DCC_PATH/config
    echo 0x182215a0 > $DCC_PATH/config
    echo 0x182215b4 > $DCC_PATH/config
    echo 0x182215c8 > $DCC_PATH/config
    echo 0x182215dc > $DCC_PATH/config
    echo 0x182215f0 > $DCC_PATH/config
    echo 0x18221604 > $DCC_PATH/config
    echo 0x18221618 > $DCC_PATH/config
    echo 0x1822162c > $DCC_PATH/config
    echo 0x18221640 > $DCC_PATH/config
    echo 0x182217b4 > $DCC_PATH/config
    echo 0x182217c8 > $DCC_PATH/config
    echo 0x182217dc > $DCC_PATH/config
    echo 0x182217f0 > $DCC_PATH/config
    echo 0x18221804 > $DCC_PATH/config
    echo 0x18221818 > $DCC_PATH/config
    echo 0x1822182c > $DCC_PATH/config
    echo 0x18221840 > $DCC_PATH/config
    echo 0x18221854 > $DCC_PATH/config
    echo 0x18221868 > $DCC_PATH/config
    echo 0x1822187c > $DCC_PATH/config
    echo 0x18221890 > $DCC_PATH/config
    echo 0x182218a4 > $DCC_PATH/config
    echo 0x182218b8 > $DCC_PATH/config
    echo 0x182218cc > $DCC_PATH/config
    echo 0x182218e0 > $DCC_PATH/config
    echo 0x18221a54 > $DCC_PATH/config
    echo 0x18221a68 > $DCC_PATH/config
    echo 0x18221a7c > $DCC_PATH/config
    echo 0x18221a90 > $DCC_PATH/config
    echo 0x18221aa4 > $DCC_PATH/config
    echo 0x18221ab8 > $DCC_PATH/config
    echo 0x18221acc > $DCC_PATH/config
    echo 0x18221ae0 > $DCC_PATH/config
    echo 0x18221af4 > $DCC_PATH/config
    echo 0x18221b08 > $DCC_PATH/config
    echo 0x18221b1c > $DCC_PATH/config
    echo 0x18221b30 > $DCC_PATH/config
    echo 0x18221b44 > $DCC_PATH/config
    echo 0x18221b58 > $DCC_PATH/config
    echo 0x18221b6c > $DCC_PATH/config
    echo 0x18221b80 > $DCC_PATH/config
    echo 0x18221cf4 > $DCC_PATH/config
    echo 0x18221d08 > $DCC_PATH/config
    echo 0x18221d1c > $DCC_PATH/config
    echo 0x18221d30 > $DCC_PATH/config
    echo 0x18221d44 > $DCC_PATH/config
    echo 0x18221d58 > $DCC_PATH/config
    echo 0x18221d6c > $DCC_PATH/config
    echo 0x18221d80 > $DCC_PATH/config
    echo 0x18221d94 > $DCC_PATH/config
    echo 0x18221da8 > $DCC_PATH/config
    echo 0x18221dbc > $DCC_PATH/config
    echo 0x18221dd0 > $DCC_PATH/config
    echo 0x18221de4 > $DCC_PATH/config
    echo 0x18221df8 > $DCC_PATH/config
    echo 0x18221e0c > $DCC_PATH/config
    echo 0x18221e20 > $DCC_PATH/config
    echo 0x18221f94 > $DCC_PATH/config
    echo 0x18221fa8 > $DCC_PATH/config
    echo 0x18221fbc > $DCC_PATH/config
    echo 0x18221fd0 > $DCC_PATH/config
    echo 0x18221fe4 > $DCC_PATH/config
    echo 0x18221ff8 > $DCC_PATH/config
    echo 0x1822200c > $DCC_PATH/config
    echo 0x18222020 > $DCC_PATH/config
    echo 0x18222034 > $DCC_PATH/config
    echo 0x18222048 > $DCC_PATH/config
    echo 0x1822205c > $DCC_PATH/config
    echo 0x18222070 > $DCC_PATH/config
    echo 0x18222084 > $DCC_PATH/config
    echo 0x18222098 > $DCC_PATH/config
    echo 0x182220ac > $DCC_PATH/config
    echo 0x182220c0 > $DCC_PATH/config
    echo 0x18221278 > $DCC_PATH/config
    echo 0x1822128c > $DCC_PATH/config
    echo 0x182212a0 > $DCC_PATH/config
    echo 0x182212b4 > $DCC_PATH/config
    echo 0x182212c8 > $DCC_PATH/config
    echo 0x182212dc > $DCC_PATH/config
    echo 0x182212f0 > $DCC_PATH/config
    echo 0x18221304 > $DCC_PATH/config
    echo 0x18221318 > $DCC_PATH/config
    echo 0x1822132c > $DCC_PATH/config
    echo 0x18221340 > $DCC_PATH/config
    echo 0x18221354 > $DCC_PATH/config
    echo 0x18221368 > $DCC_PATH/config
    echo 0x1822137c > $DCC_PATH/config
    echo 0x18221390 > $DCC_PATH/config
    echo 0x182213a4 > $DCC_PATH/config
    echo 0x18221518 > $DCC_PATH/config
    echo 0x1822152c > $DCC_PATH/config
    echo 0x18221540 > $DCC_PATH/config
    echo 0x18221554 > $DCC_PATH/config
    echo 0x18221568 > $DCC_PATH/config
    echo 0x1822157c > $DCC_PATH/config
    echo 0x18221590 > $DCC_PATH/config
    echo 0x182215a4 > $DCC_PATH/config
    echo 0x182215b8 > $DCC_PATH/config
    echo 0x182215cc > $DCC_PATH/config
    echo 0x182215e0 > $DCC_PATH/config
    echo 0x182215f4 > $DCC_PATH/config
    echo 0x18221608 > $DCC_PATH/config
    echo 0x1822161c > $DCC_PATH/config
    echo 0x18221630 > $DCC_PATH/config
    echo 0x18221644 > $DCC_PATH/config
    echo 0x182217b8 > $DCC_PATH/config
    echo 0x182217cc > $DCC_PATH/config
    echo 0x182217e0 > $DCC_PATH/config
    echo 0x182217f4 > $DCC_PATH/config
    echo 0x18221808 > $DCC_PATH/config
    echo 0x1822181c > $DCC_PATH/config
    echo 0x18221830 > $DCC_PATH/config
    echo 0x18221844 > $DCC_PATH/config
    echo 0x18221858 > $DCC_PATH/config
    echo 0x1822186c > $DCC_PATH/config
    echo 0x18221880 > $DCC_PATH/config
    echo 0x18221894 > $DCC_PATH/config
    echo 0x182218a8 > $DCC_PATH/config
    echo 0x182218bc > $DCC_PATH/config
    echo 0x182218d0 > $DCC_PATH/config
    echo 0x182218e4 > $DCC_PATH/config
    echo 0x18221a58 > $DCC_PATH/config
    echo 0x18221a6c > $DCC_PATH/config
    echo 0x18221a80 > $DCC_PATH/config
    echo 0x18221a94 > $DCC_PATH/config
    echo 0x18221aa8 > $DCC_PATH/config
    echo 0x18221abc > $DCC_PATH/config
    echo 0x18221ad0 > $DCC_PATH/config
    echo 0x18221ae4 > $DCC_PATH/config
    echo 0x18221af8 > $DCC_PATH/config
    echo 0x18221b0c > $DCC_PATH/config
    echo 0x18221b20 > $DCC_PATH/config
    echo 0x18221b34 > $DCC_PATH/config
    echo 0x18221b48 > $DCC_PATH/config
    echo 0x18221b5c > $DCC_PATH/config
    echo 0x18221b70 > $DCC_PATH/config
    echo 0x18221b84 > $DCC_PATH/config
    echo 0x18221cf8 > $DCC_PATH/config
    echo 0x18221d0c > $DCC_PATH/config
    echo 0x18221d20 > $DCC_PATH/config
    echo 0x18221d34 > $DCC_PATH/config
    echo 0x18221d48 > $DCC_PATH/config
    echo 0x18221d5c > $DCC_PATH/config
    echo 0x18221d70 > $DCC_PATH/config
    echo 0x18221d84 > $DCC_PATH/config
    echo 0x18221d98 > $DCC_PATH/config
    echo 0x18221dac > $DCC_PATH/config
    echo 0x18221dc0 > $DCC_PATH/config
    echo 0x18221dd4 > $DCC_PATH/config
    echo 0x18221de8 > $DCC_PATH/config
    echo 0x18221dfc > $DCC_PATH/config
    echo 0x18221e10 > $DCC_PATH/config
    echo 0x18221e24 > $DCC_PATH/config
    echo 0x18221f98 > $DCC_PATH/config
    echo 0x18221fac > $DCC_PATH/config
    echo 0x18221fc0 > $DCC_PATH/config
    echo 0x18221fd4 > $DCC_PATH/config
    echo 0x18221fe8 > $DCC_PATH/config
    echo 0x18221ffc > $DCC_PATH/config
    echo 0x18222010 > $DCC_PATH/config
    echo 0x18222024 > $DCC_PATH/config
    echo 0x18222038 > $DCC_PATH/config
    echo 0x1822204c > $DCC_PATH/config
    echo 0x18222060 > $DCC_PATH/config
    echo 0x18222074 > $DCC_PATH/config
    echo 0x18222088 > $DCC_PATH/config
    echo 0x1822209c > $DCC_PATH/config
    echo 0x182220b0 > $DCC_PATH/config
    echo 0x182220c4 > $DCC_PATH/config
}

config_atoll_dcc_lpm()
{
    #LPM_Registers
    echo 0x18000024 > $DCC_PATH/config
    echo 0x18000040 > $DCC_PATH/config
    echo 0x18010024 > $DCC_PATH/config
    echo 0x18010040 > $DCC_PATH/config
    echo 0x18020024 > $DCC_PATH/config
    echo 0x18020040 > $DCC_PATH/config
    echo 0x18030024 > $DCC_PATH/config
    echo 0x18030040 > $DCC_PATH/config
    echo 0x18040024 > $DCC_PATH/config
    echo 0x18040040 > $DCC_PATH/config
    echo 0x18050024 > $DCC_PATH/config
    echo 0x18050040 > $DCC_PATH/config
    echo 0x18060024 > $DCC_PATH/config
    echo 0x18060040 > $DCC_PATH/config
    echo 0x18070024 > $DCC_PATH/config
    echo 0x18070040 > $DCC_PATH/config
    echo 0x18080024 > $DCC_PATH/config
    echo 0x18080040 > $DCC_PATH/config
    echo 0x180800F8 > $DCC_PATH/config
    echo 0x18080104 > $DCC_PATH/config
    echo 0x1808011C > $DCC_PATH/config
    echo 0x18080128 > $DCC_PATH/config
}

config_atoll_dcc_rscc_lpass()
{
    #RSCp
    echo 0x62900010  > $DCC_PATH/config
    echo 0x62900014  > $DCC_PATH/config
    echo 0x62900018  > $DCC_PATH/config
    echo 0x62900030  > $DCC_PATH/config
    echo 0x62900038  > $DCC_PATH/config
    echo 0x62900040  > $DCC_PATH/config
    echo 0x62900048  > $DCC_PATH/config
    echo 0x629000D0  > $DCC_PATH/config
    echo 0x62900210  > $DCC_PATH/config
    echo 0x62900230  > $DCC_PATH/config
    echo 0x62900250  > $DCC_PATH/config
    echo 0x62900270  > $DCC_PATH/config
    echo 0x62900290  > $DCC_PATH/config
    echo 0x629002B0  > $DCC_PATH/config
    echo 0x62900208  > $DCC_PATH/config
    echo 0x62900228  > $DCC_PATH/config
    echo 0x62900248  > $DCC_PATH/config
    echo 0x62900268  > $DCC_PATH/config
    echo 0x62900288  > $DCC_PATH/config
    echo 0x629002A8  > $DCC_PATH/config
    echo 0x6290020C  > $DCC_PATH/config
    echo 0x6290022C  > $DCC_PATH/config
    echo 0x6290024C  > $DCC_PATH/config
    echo 0x6290026C  > $DCC_PATH/config
    echo 0x6290028C  > $DCC_PATH/config
    echo 0x629002AC  > $DCC_PATH/config
    echo 0x62900404  > $DCC_PATH/config
    echo 0x62900408  > $DCC_PATH/config
    echo 0x62900400  > $DCC_PATH/config
    echo 0x62900D04  > $DCC_PATH/config

    #RSCc
    echo 0x624B0010 > $DCC_PATH/config
    echo 0x624B0014 > $DCC_PATH/config
    echo 0x624B0018 > $DCC_PATH/config
    echo 0x624B0210 > $DCC_PATH/config
    echo 0x624B0230 > $DCC_PATH/config
    echo 0x624B0250 > $DCC_PATH/config
    echo 0x624B0270 > $DCC_PATH/config
    echo 0x624B0290 > $DCC_PATH/config
    echo 0x624B02B0 > $DCC_PATH/config
    echo 0x624B0208 > $DCC_PATH/config
    echo 0x624B0228 > $DCC_PATH/config
    echo 0x624B0248 > $DCC_PATH/config
    echo 0x624B0268 > $DCC_PATH/config
    echo 0x624B0288 > $DCC_PATH/config
    echo 0x624B02A8 > $DCC_PATH/config
    echo 0x624B020C > $DCC_PATH/config
    echo 0x624B022C > $DCC_PATH/config
    echo 0x624B024C > $DCC_PATH/config
    echo 0x624B026C > $DCC_PATH/config
    echo 0x624B028C > $DCC_PATH/config
    echo 0x624B02AC > $DCC_PATH/config
    echo 0x624B0400 > $DCC_PATH/config
    echo 0x624B0404 > $DCC_PATH/config
    echo 0x624B0408 > $DCC_PATH/config

    #Q6_Status
    echo 0x62402028 > $DCC_PATH/config

    #PDC registers
    echo 0xB254520 > $DCC_PATH/config
    echo 0xB251020 > $DCC_PATH/config
    echo 0xB251024 > $DCC_PATH/config
    echo 0xB251030 > $DCC_PATH/config
    echo 0xB251200 > $DCC_PATH/config
    echo 0xB251214 > $DCC_PATH/config
    echo 0xB251228 > $DCC_PATH/config
    echo 0xB25123C > $DCC_PATH/config
    echo 0xB251250 > $DCC_PATH/config
    echo 0xB251204 > $DCC_PATH/config
    echo 0xB251218 > $DCC_PATH/config
    echo 0xB25122C > $DCC_PATH/config
    echo 0xB251240 > $DCC_PATH/config
    echo 0xB251254 > $DCC_PATH/config
    echo 0xB251208 > $DCC_PATH/config
    echo 0xB25121C > $DCC_PATH/config
    echo 0xB251230 > $DCC_PATH/config
    echo 0xB251244 > $DCC_PATH/config
    echo 0xB251258 > $DCC_PATH/config
    echo 0xB254510 > $DCC_PATH/config
    echo 0xB254514 > $DCC_PATH/config
    echo 0xB250010 > $DCC_PATH/config
    echo 0xB250014 > $DCC_PATH/config
    echo 0xB250900 > $DCC_PATH/config
    echo 0xB250904 > $DCC_PATH/config
}

config_atoll_dcc_rscc_modem()
{
    #RSCp
    echo 0x4200010 > $DCC_PATH/config
    echo 0x4200014 > $DCC_PATH/config
    echo 0x4200018 > $DCC_PATH/config
    echo 0x4200030 > $DCC_PATH/config
    echo 0x4200038 > $DCC_PATH/config
    echo 0x4200040 > $DCC_PATH/config
    echo 0x4200048 > $DCC_PATH/config
    echo 0x42000D0 > $DCC_PATH/config
    echo 0x4200210 > $DCC_PATH/config
    echo 0x4200230 > $DCC_PATH/config
    echo 0x4200250 > $DCC_PATH/config
    echo 0x4200270 > $DCC_PATH/config
    echo 0x4200290 > $DCC_PATH/config
    echo 0x42002B0 > $DCC_PATH/config
    echo 0x4200208 > $DCC_PATH/config
    echo 0x4200228 > $DCC_PATH/config
    echo 0x4200248 > $DCC_PATH/config
    echo 0x4200268 > $DCC_PATH/config
    echo 0x4200288 > $DCC_PATH/config
    echo 0x42002A8 > $DCC_PATH/config
    echo 0x420020C > $DCC_PATH/config
    echo 0x420022C > $DCC_PATH/config
    echo 0x420024C > $DCC_PATH/config
    echo 0x420026C > $DCC_PATH/config
    echo 0x420028C > $DCC_PATH/config
    echo 0x42002AC > $DCC_PATH/config
    echo 0x4200404 > $DCC_PATH/config
    echo 0x4200408 > $DCC_PATH/config
    echo 0x4200400 > $DCC_PATH/config
    echo 0x4200D04 > $DCC_PATH/config

    #RSCc
    echo 0x4130010 > $DCC_PATH/config
    echo 0x4130014 > $DCC_PATH/config
    echo 0x4130018 > $DCC_PATH/config
    echo 0x4130210 > $DCC_PATH/config
    echo 0x4130230 > $DCC_PATH/config
    echo 0x4130250 > $DCC_PATH/config
    echo 0x4130270 > $DCC_PATH/config
    echo 0x4130290 > $DCC_PATH/config
    echo 0x41302B0 > $DCC_PATH/config
    echo 0x4130208 > $DCC_PATH/config
    echo 0x4130228 > $DCC_PATH/config
    echo 0x4130248 > $DCC_PATH/config
    echo 0x4130268 > $DCC_PATH/config
    echo 0x4130288 > $DCC_PATH/config
    echo 0x41302A8 > $DCC_PATH/config
    echo 0x413020C > $DCC_PATH/config
    echo 0x413022C > $DCC_PATH/config
    echo 0x413024C > $DCC_PATH/config
    echo 0x413026C > $DCC_PATH/config
    echo 0x413028C > $DCC_PATH/config
    echo 0x41302AC > $DCC_PATH/config
    echo 0x4130400 > $DCC_PATH/config
    echo 0x4130404 > $DCC_PATH/config
    echo 0x4130408 > $DCC_PATH/config

    #Q6 status
    echo 0x4082028  > $DCC_PATH/config
    echo 0x0018A008 > $DCC_PATH/config

    #PDC registers
    echo 0xB2C4520 > $DCC_PATH/config
    echo 0xB2C1020 > $DCC_PATH/config
    echo 0xB2C1024 > $DCC_PATH/config
    echo 0xB2C1030 > $DCC_PATH/config
    echo 0xB2C1200 > $DCC_PATH/config
    echo 0xB2C1214 > $DCC_PATH/config
    echo 0xB2C1228 > $DCC_PATH/config
    echo 0xB2C123C > $DCC_PATH/config
    echo 0xB2C1250 > $DCC_PATH/config
    echo 0xB2C1204 > $DCC_PATH/config
    echo 0xB2C1218 > $DCC_PATH/config
    echo 0xB2C122C > $DCC_PATH/config
    echo 0xB2C1240 > $DCC_PATH/config
    echo 0xB2C1254 > $DCC_PATH/config
    echo 0xB2C1208 > $DCC_PATH/config
    echo 0xB2C121C > $DCC_PATH/config
    echo 0xB2C1230 > $DCC_PATH/config
    echo 0xB2C1244 > $DCC_PATH/config
    echo 0xB2C1258 > $DCC_PATH/config
    echo 0xB2C4510 > $DCC_PATH/config
    echo 0xB2C4514 > $DCC_PATH/config
    echo 0xB2C0010 > $DCC_PATH/config
    echo 0xB2C0014 > $DCC_PATH/config
    echo 0xB2C0900 > $DCC_PATH/config
    echo 0xB2C0904 > $DCC_PATH/config
}

config_atoll_dcc_rscc_cdsp()
{
    #RSCp
    echo 0x80A4010 > $DCC_PATH/config
    echo 0x80A4014 > $DCC_PATH/config
    echo 0x80A4018 > $DCC_PATH/config
    echo 0x80A4030 > $DCC_PATH/config
    echo 0x80A4038 > $DCC_PATH/config
    echo 0x80A4040 > $DCC_PATH/config
    echo 0x80A4048 > $DCC_PATH/config
    echo 0x80A40D0 > $DCC_PATH/config
    echo 0x80A4210 > $DCC_PATH/config
    echo 0x80A4230 > $DCC_PATH/config
    echo 0x80A4250 > $DCC_PATH/config
    echo 0x80A4270 > $DCC_PATH/config
    echo 0x80A4290 > $DCC_PATH/config
    echo 0x80A42B0 > $DCC_PATH/config
    echo 0x80A4208 > $DCC_PATH/config
    echo 0x80A4228 > $DCC_PATH/config
    echo 0x80A4248 > $DCC_PATH/config
    echo 0x80A4268 > $DCC_PATH/config
    echo 0x80A4288 > $DCC_PATH/config
    echo 0x80A42A8 > $DCC_PATH/config
    echo 0x80A420C > $DCC_PATH/config
    echo 0x80A422C > $DCC_PATH/config
    echo 0x80A424C > $DCC_PATH/config
    echo 0x80A426C > $DCC_PATH/config
    echo 0x80A428C > $DCC_PATH/config
    echo 0x80A42AC > $DCC_PATH/config
    echo 0x80A4404 > $DCC_PATH/config
    echo 0x80A4408 > $DCC_PATH/config
    echo 0x80A4400 > $DCC_PATH/config
    echo 0x80A4D04 > $DCC_PATH/config

    #RSCc_CDSP
    echo 0x83B0010 > $DCC_PATH/config
    echo 0x83B0014 > $DCC_PATH/config
    echo 0x83B0018 > $DCC_PATH/config
    echo 0x83B0210 > $DCC_PATH/config
    echo 0x83B0230 > $DCC_PATH/config
    echo 0x83B0250 > $DCC_PATH/config
    echo 0x83B0270 > $DCC_PATH/config
    echo 0x83B0290 > $DCC_PATH/config
    echo 0x83B02B0 > $DCC_PATH/config
    echo 0x83B0208 > $DCC_PATH/config
    echo 0x83B0228 > $DCC_PATH/config
    echo 0x83B0248 > $DCC_PATH/config
    echo 0x83B0268 > $DCC_PATH/config
    echo 0x83B0288 > $DCC_PATH/config
    echo 0x83B02A8 > $DCC_PATH/config
    echo 0x83B020C > $DCC_PATH/config
    echo 0x83B022C > $DCC_PATH/config
    echo 0x83B024C > $DCC_PATH/config
    echo 0x83B026C > $DCC_PATH/config
    echo 0x83B028C > $DCC_PATH/config
    echo 0x83B02AC > $DCC_PATH/config
    echo 0x83B0400 > $DCC_PATH/config
    echo 0x83B0404 > $DCC_PATH/config
    echo 0x83B0408 > $DCC_PATH/config

    #Q6 Status
    echo 0x8302028 > $DCC_PATH/config

    #PDC
    echo 0xB2B4520> $DCC_PATH/config
    echo 0xB2B1020> $DCC_PATH/config
    echo 0xB2B1024> $DCC_PATH/config
    echo 0xB2B1030> $DCC_PATH/config
    echo 0xB2B1200> $DCC_PATH/config
    echo 0xB2B1214> $DCC_PATH/config
    echo 0xB2B1228> $DCC_PATH/config
    echo 0xB2B123C> $DCC_PATH/config
    echo 0xB2B1250> $DCC_PATH/config
    echo 0xB2B1204> $DCC_PATH/config
    echo 0xB2B1218> $DCC_PATH/config
    echo 0xB2B122C> $DCC_PATH/config
    echo 0xB2B1240> $DCC_PATH/config
    echo 0xB2B1254> $DCC_PATH/config
    echo 0xB2B1208> $DCC_PATH/config
    echo 0xB2B121C> $DCC_PATH/config
    echo 0xB2B1230> $DCC_PATH/config
    echo 0xB2B1244> $DCC_PATH/config
    echo 0xB2B1258> $DCC_PATH/config
    echo 0xB2B4510> $DCC_PATH/config
    echo 0xB2B4514> $DCC_PATH/config
    echo 0xB2B0010> $DCC_PATH/config
    echo 0xB2B0014> $DCC_PATH/config
    echo 0xB2B0900> $DCC_PATH/config
    echo 0xB2B0904> $DCC_PATH/config
}

config_atoll_dcc_axi_pc()
{
    #AXI_PC
}

config_atoll_dcc_apb_pc()
{
    #APB_PC
}

config_atoll_dcc_memnoc_mccc()
{
    #MCCC
    echo 0x90b0280 > $DCC_PATH/config
    echo 0x90b0288 > $DCC_PATH/config
    echo 0x90b028c > $DCC_PATH/config
    echo 0x90b0290 > $DCC_PATH/config
    echo 0x90b0294 > $DCC_PATH/config
    echo 0x90b0298 > $DCC_PATH/config
    echo 0x90b029c > $DCC_PATH/config
    echo 0x90b02a0 > $DCC_PATH/config
    echo 0x90b02a0 4 > $DCC_PATH/config
    echo 0x92d0110 4 > $DCC_PATH/config
}

config_atoll_dcc_pdc_display()
{
    #PDC_DISPLAY
}

config_atoll_dcc_aop_rpmh()
{
    #AOP_RPMH &  TCS

}

config_atoll_dcc_lmh()
{
    #LMH
}

config_atoll_dcc_ipm_apps()
{
    #IPM_APPS
}

config_atoll_dcc_osm()
{
    #APSS_OSM
    echo 0x18321700 > $DCC_PATH/config
    echo 0x18322C18 > $DCC_PATH/config
    echo 0x18323700 > $DCC_PATH/config
    echo 0x18324C18 > $DCC_PATH/config
    echo 0x18325F00 > $DCC_PATH/config
    echo 0x18327418 > $DCC_PATH/config
    echo 0x18321818 > $DCC_PATH/config
    echo 0x18323818 > $DCC_PATH/config
    echo 0x18326018 > $DCC_PATH/config
    echo 0x18321920 > $DCC_PATH/config
    echo 0x1832102C > $DCC_PATH/config
    echo 0x18321044 > $DCC_PATH/config
    echo 0x18321710 > $DCC_PATH/config
    echo 0x1832176C > $DCC_PATH/config
    echo 0x18322C18 > $DCC_PATH/config
    echo 0x18323700 > $DCC_PATH/config
    echo 0x18323920 > $DCC_PATH/config
    echo 0x1832302C > $DCC_PATH/config
    echo 0x18323044 > $DCC_PATH/config
    echo 0x18323710 > $DCC_PATH/config
    echo 0x1832376C > $DCC_PATH/config
    echo 0x18324C18 > $DCC_PATH/config
    echo 0x18326120 > $DCC_PATH/config
    echo 0x1832582C > $DCC_PATH/config
    echo 0x18325844 > $DCC_PATH/config
    echo 0x18325F10 > $DCC_PATH/config
    echo 0x18325F6C > $DCC_PATH/config
    echo 0x18327418 > $DCC_PATH/config
    echo 0x1832582C > $DCC_PATH/config
    echo 0x18280000 2 > $DCC_PATH/config
    echo 0x18282000 2 > $DCC_PATH/config
    echo 0x18284000 2 > $DCC_PATH/config
}

config_atoll_dcc_ddr()
{
    #DDR GEMNOC TR PEnding status
    echo 0x90c012c > $DCC_PATH/config
    #LLCC Registers
    echo 0x9222408 > $DCC_PATH/config
    echo 0x9220344 2 > $DCC_PATH/config
    echo 0x9220480 > $DCC_PATH/config
    echo 0x922358c > $DCC_PATH/config
    echo 0x9222398 > $DCC_PATH/config
    echo 0x92223a4 > $DCC_PATH/config
    echo 0x92223a4 > $DCC_PATH/config
    echo 0x92223a4 > $DCC_PATH/config
    echo 0x92223a4 > $DCC_PATH/config
    echo 0x92223a4 > $DCC_PATH/config
    echo 0x92223a4 > $DCC_PATH/config
    echo 0x923201c 5 > $DCC_PATH/config
    echo 0x9232050 > $DCC_PATH/config
    #LLCC0_LLCC_FEWC_FIFO_STATUS
    echo 0x9232100  > $DCC_PATH/config
    #DDR CLK registers
    echo 0x9186048 > $DCC_PATH/config
    echo 0x9186054 > $DCC_PATH/config
    echo 0x9186164 > $DCC_PATH/config
    echo 0x9186170 > $DCC_PATH/config
}

config_atoll_dcc_ecc_llc()
{
    #ECC_LLC
}

config_atoll_dcc_cabo_llcc_shrm()
{
    #LLCC/CABO
    echo 0x9236028 > $DCC_PATH/config
    echo 0x923602C > $DCC_PATH/config
    echo 0x9236030 > $DCC_PATH/config
    echo 0x9236034 > $DCC_PATH/config
    echo 0x9236038 > $DCC_PATH/config
    echo 0x9232100 > $DCC_PATH/config
    echo 0x92360b0 > $DCC_PATH/config
    echo 0x9236044 > $DCC_PATH/config
    echo 0x9236048 > $DCC_PATH/config
    echo 0x923604c > $DCC_PATH/config
    echo 0x9236050 > $DCC_PATH/config
    echo 0x923e030 > $DCC_PATH/config
    echo 0x923e034 > $DCC_PATH/config
    echo 0x9241000 > $DCC_PATH/config
    echo 0x9248058 > $DCC_PATH/config
    echo 0x924805c > $DCC_PATH/config
    echo 0x9248060 > $DCC_PATH/config
    echo 0x9248064 > $DCC_PATH/config

    echo 0x9260410 > $DCC_PATH/config
    echo 0x92e0410 > $DCC_PATH/config
    echo 0x9260414 > $DCC_PATH/config
    echo 0x92e0414 > $DCC_PATH/config
    echo 0x9260418 > $DCC_PATH/config
    echo 0x92e0418 > $DCC_PATH/config
    echo 0x9260420 > $DCC_PATH/config
    echo 0x9260424 > $DCC_PATH/config
    echo 0x9260430 > $DCC_PATH/config
    echo 0x9260440 > $DCC_PATH/config
    echo 0x9260448 > $DCC_PATH/config
    echo 0x92604a0 > $DCC_PATH/config
    echo 0x92604b0 > $DCC_PATH/config
    echo 0x92604d0 2 > $DCC_PATH/config
    echo 0x9261440 > $DCC_PATH/config
    echo 0x92e0420 > $DCC_PATH/config
    echo 0x92e0424 > $DCC_PATH/config
    echo 0x92e0430 > $DCC_PATH/config
    echo 0x92e0440 > $DCC_PATH/config
    echo 0x92e0448 > $DCC_PATH/config
    echo 0x92e04a0 > $DCC_PATH/config
    echo 0x92e04b0 > $DCC_PATH/config
    echo 0x92e04d0 2 > $DCC_PATH/config
    #LLCC Broadcast
    echo 0x9600000 > $DCC_PATH/config
    echo 0x9601000 > $DCC_PATH/config
    echo 0x9602000 > $DCC_PATH/config
    echo 0x9603000 > $DCC_PATH/config
    echo 0x9604000 > $DCC_PATH/config
    echo 0x9605000 > $DCC_PATH/config
    echo 0x9606000 > $DCC_PATH/config
    echo 0x9607000 > $DCC_PATH/config
    echo 0x9608000 > $DCC_PATH/config
    echo 0x9609000 > $DCC_PATH/config
    echo 0x960a000 > $DCC_PATH/config
    echo 0x960b000 > $DCC_PATH/config
    echo 0x960c000 > $DCC_PATH/config
    echo 0x960d000 > $DCC_PATH/config
    echo 0x960e000 > $DCC_PATH/config
    echo 0x960f000 > $DCC_PATH/config
    echo 0x9610000 > $DCC_PATH/config
    echo 0x9611000 > $DCC_PATH/config
    echo 0x9612000 > $DCC_PATH/config
    echo 0x9613000 > $DCC_PATH/config
    echo 0x9614000 > $DCC_PATH/config
    echo 0x9615000 > $DCC_PATH/config
    echo 0x9616000 > $DCC_PATH/config
    echo 0x9617000 > $DCC_PATH/config
    echo 0x9618000 > $DCC_PATH/config
    echo 0x9619000 > $DCC_PATH/config
    echo 0x961a000 > $DCC_PATH/config
    echo 0x961b000 > $DCC_PATH/config
    echo 0x961c000 > $DCC_PATH/config
    echo 0x961d000 > $DCC_PATH/config
    echo 0x961e000 > $DCC_PATH/config
    echo 0x961f000 > $DCC_PATH/config
    echo 0x9600004 > $DCC_PATH/config
    echo 0x9601004 > $DCC_PATH/config
    echo 0x9602004 > $DCC_PATH/config
    echo 0x9603004 > $DCC_PATH/config
    echo 0x9604004 > $DCC_PATH/config
    echo 0x9605004 > $DCC_PATH/config
    echo 0x9606004 > $DCC_PATH/config
    echo 0x9607004 > $DCC_PATH/config
    echo 0x9608004 > $DCC_PATH/config
    echo 0x9609004 > $DCC_PATH/config
    echo 0x960a004 > $DCC_PATH/config
    echo 0x960b004 > $DCC_PATH/config
    echo 0x960c004 > $DCC_PATH/config
    echo 0x960d004 > $DCC_PATH/config
    echo 0x960e004 > $DCC_PATH/config
    echo 0x960f004 > $DCC_PATH/config
    echo 0x9610004 > $DCC_PATH/config
    echo 0x9611004 > $DCC_PATH/config
    echo 0x9612004 > $DCC_PATH/config
    echo 0x9613004 > $DCC_PATH/config
    echo 0x9614004 > $DCC_PATH/config
    echo 0x9615004 > $DCC_PATH/config
    echo 0x9616004 > $DCC_PATH/config
    echo 0x9617004 > $DCC_PATH/config
    echo 0x9618004 > $DCC_PATH/config
    echo 0x9619004 > $DCC_PATH/config
    echo 0x961a004 > $DCC_PATH/config
    echo 0x961b004 > $DCC_PATH/config
    echo 0x961c004 > $DCC_PATH/config
    echo 0x961d004 > $DCC_PATH/config
    echo 0x961e004 > $DCC_PATH/config
    echo 0x961f004 > $DCC_PATH/config

    echo 0x9266418 > $DCC_PATH/config
    echo 0x92e6418 > $DCC_PATH/config
    echo 0x9265804 > $DCC_PATH/config
    echo 0x92e5804 > $DCC_PATH/config

    echo 0x92604b8 > $DCC_PATH/config
    echo 0x92e04b8 > $DCC_PATH/config

}

config_atoll_dcc_cx_mx()
{
    #CX_MX
    echo 0x0C201244 1 > $DCC_PATH/config
    echo 0x0C202244 1 > $DCC_PATH/config
    #APC Voltage
    echo 0x18100C18 1 > $DCC_PATH/config
    echo 0x18101C18 1 > $DCC_PATH/config
    #APC / MX CORNER
    echo 0x18300000 1 > $DCC_PATH/config
    #CPRH
    echo 0x183A3A84 2 > $DCC_PATH/config
    echo 0x18393A84 1 > $DCC_PATH/config
}

config_atoll_dcc_gcc_regs()
{
    #GCC
    echo 0x00100000 > $DCC_PATH/config
    echo 0x00100004 > $DCC_PATH/config
    echo 0x00100008 > $DCC_PATH/config
    echo 0x0010000C > $DCC_PATH/config
    echo 0x00100010 > $DCC_PATH/config
    echo 0x00100014 > $DCC_PATH/config
    echo 0x00100018 > $DCC_PATH/config
    echo 0x0010001C > $DCC_PATH/config
    echo 0x00100020 > $DCC_PATH/config
    echo 0x00100024 > $DCC_PATH/config
    echo 0x00100028 > $DCC_PATH/config
    echo 0x0010002C > $DCC_PATH/config
    echo 0x00100030 > $DCC_PATH/config
    echo 0x00100034 > $DCC_PATH/config
    echo 0x00100100 > $DCC_PATH/config
    echo 0x00100104 > $DCC_PATH/config
    echo 0x00100108 > $DCC_PATH/config
    echo 0x0010010C > $DCC_PATH/config
    echo 0x00101000 > $DCC_PATH/config
    echo 0x00101004 > $DCC_PATH/config
    echo 0x00101008 > $DCC_PATH/config
    echo 0x0010100C > $DCC_PATH/config
    echo 0x00101010 > $DCC_PATH/config
    echo 0x00101014 > $DCC_PATH/config
    echo 0x00101018 > $DCC_PATH/config
    echo 0x0010101C > $DCC_PATH/config
    echo 0x00101020 > $DCC_PATH/config
    echo 0x00101024 > $DCC_PATH/config
    echo 0x00101028 > $DCC_PATH/config
    echo 0x0010102C > $DCC_PATH/config
    echo 0x00101030 > $DCC_PATH/config
    echo 0x00101034 > $DCC_PATH/config
    echo 0x00102000 > $DCC_PATH/config
    echo 0x00102004 > $DCC_PATH/config
    echo 0x00102008 > $DCC_PATH/config
    echo 0x0010200C > $DCC_PATH/config
    echo 0x00102010 > $DCC_PATH/config
    echo 0x00102014 > $DCC_PATH/config
    echo 0x00102018 > $DCC_PATH/config
    echo 0x0010201C > $DCC_PATH/config
    echo 0x00102020 > $DCC_PATH/config
    echo 0x00102024 > $DCC_PATH/config
    echo 0x00102028 > $DCC_PATH/config
    echo 0x0010202C > $DCC_PATH/config
    echo 0x00102030 > $DCC_PATH/config
    echo 0x00102034 > $DCC_PATH/config
    echo 0x00103000 > $DCC_PATH/config
    echo 0x00103004 > $DCC_PATH/config
    echo 0x00103008 > $DCC_PATH/config
    echo 0x0010300C > $DCC_PATH/config
    echo 0x00103010 > $DCC_PATH/config
    echo 0x00103014 > $DCC_PATH/config
    echo 0x00103018 > $DCC_PATH/config
    echo 0x0010301C > $DCC_PATH/config
    echo 0x00103020 > $DCC_PATH/config
    echo 0x00103024 > $DCC_PATH/config
    echo 0x00103028 > $DCC_PATH/config
    echo 0x0010302C > $DCC_PATH/config
    echo 0x00103030 > $DCC_PATH/config
    echo 0x00103034 > $DCC_PATH/config
    echo 0x00113000 > $DCC_PATH/config
    echo 0x00113004 > $DCC_PATH/config
    echo 0x00113008 > $DCC_PATH/config
    echo 0x0011300C > $DCC_PATH/config
    echo 0x00113010 > $DCC_PATH/config
    echo 0x00113014 > $DCC_PATH/config
    echo 0x00113018 > $DCC_PATH/config
    echo 0x0011301C > $DCC_PATH/config
    echo 0x00113020 > $DCC_PATH/config
    echo 0x00113024 > $DCC_PATH/config
    echo 0x00113028 > $DCC_PATH/config
    echo 0x0011302C > $DCC_PATH/config
    echo 0x00113030 > $DCC_PATH/config
    echo 0x00113034 > $DCC_PATH/config
    echo 0x0011A000 > $DCC_PATH/config
    echo 0x0011A004 > $DCC_PATH/config
    echo 0x0011A008 > $DCC_PATH/config
    echo 0x0011A00C > $DCC_PATH/config
    echo 0x0011A010 > $DCC_PATH/config
    echo 0x0011A014 > $DCC_PATH/config
    echo 0x0011A018 > $DCC_PATH/config
    echo 0x0011A01C > $DCC_PATH/config
    echo 0x0011A020 > $DCC_PATH/config
    echo 0x0011A024 > $DCC_PATH/config
    echo 0x0011A028 > $DCC_PATH/config
    echo 0x0011A02C > $DCC_PATH/config
    echo 0x0011A030 > $DCC_PATH/config
    echo 0x0011A034 > $DCC_PATH/config
    echo 0x0011B000 > $DCC_PATH/config
    echo 0x0011B004 > $DCC_PATH/config
    echo 0x0011B008 > $DCC_PATH/config
    echo 0x0011B00C > $DCC_PATH/config
    echo 0x0011B010 > $DCC_PATH/config
    echo 0x0011B014 > $DCC_PATH/config
    echo 0x0011B018 > $DCC_PATH/config
    echo 0x0011B01C > $DCC_PATH/config
    echo 0x0011B020 > $DCC_PATH/config
    echo 0x0011B024 > $DCC_PATH/config
    echo 0x0011B028 > $DCC_PATH/config
    echo 0x0011B02C > $DCC_PATH/config
    echo 0x0011B030 > $DCC_PATH/config
    echo 0x0011B034 > $DCC_PATH/config
    echo 0x00174000 > $DCC_PATH/config
    echo 0x00174004 > $DCC_PATH/config
    echo 0x00174008 > $DCC_PATH/config
    echo 0x0017400C > $DCC_PATH/config
    echo 0x00174010 > $DCC_PATH/config
    echo 0x00174014 > $DCC_PATH/config
    echo 0x00174018 > $DCC_PATH/config
    echo 0x0017401C > $DCC_PATH/config
    echo 0x00174020 > $DCC_PATH/config
    echo 0x00174024 > $DCC_PATH/config
    echo 0x00174028 > $DCC_PATH/config
    echo 0x0017402C > $DCC_PATH/config
    echo 0x00174030 > $DCC_PATH/config
    echo 0x00174034 > $DCC_PATH/config
    echo 0x00176000 > $DCC_PATH/config
    echo 0x00176004 > $DCC_PATH/config
    echo 0x00176008 > $DCC_PATH/config
    echo 0x0017600C > $DCC_PATH/config
    echo 0x00176010 > $DCC_PATH/config
    echo 0x00176014 > $DCC_PATH/config
    echo 0x00176018 > $DCC_PATH/config
    echo 0x0017601C > $DCC_PATH/config
    echo 0x00176020 > $DCC_PATH/config
    echo 0x00176024 > $DCC_PATH/config
    echo 0x00176028 > $DCC_PATH/config
    echo 0x0017602C > $DCC_PATH/config
    echo 0x00176030 > $DCC_PATH/config
    echo 0x00176034 > $DCC_PATH/config
    echo 0x0010401C > $DCC_PATH/config
    echo 0x00183024 > $DCC_PATH/config
    echo 0x00144168 > $DCC_PATH/config
    echo 0x0011702C > $DCC_PATH/config
    echo 0x0010904C > $DCC_PATH/config
    echo 0x00189038 > $DCC_PATH/config
    echo 0x001443E8 > $DCC_PATH/config
    echo 0x001442B8 > $DCC_PATH/config
    echo 0x00105060 > $DCC_PATH/config
    echo 0x00141024 > $DCC_PATH/config
    echo 0x00145038 > $DCC_PATH/config
    echo 0x00109004 > $DCC_PATH/config
    echo 0x00189004 > $DCC_PATH/config
    echo 0x00190004 > $DCC_PATH/config
    #AOSS_CC
    echo 0x0C2A0000 > $DCC_PATH/config
    echo 0x0C2A0004 > $DCC_PATH/config
    echo 0x0C2A0008 > $DCC_PATH/config
    echo 0x0C2A000C > $DCC_PATH/config
    echo 0x0C2A0010 > $DCC_PATH/config
    echo 0x0C2A0014 > $DCC_PATH/config
    echo 0x0C2A0018 > $DCC_PATH/config
    echo 0x0C2A001C > $DCC_PATH/config
    echo 0x0C2A0020 > $DCC_PATH/config
    echo 0x0C2A0024 > $DCC_PATH/config
    echo 0x0C2A0028 > $DCC_PATH/config
    echo 0x0C2A002C > $DCC_PATH/config
    echo 0x0C2A0030 > $DCC_PATH/config
    echo 0x0C2A0034 > $DCC_PATH/config
    echo 0x0C2A1000 > $DCC_PATH/config
    echo 0x0C2A1004 > $DCC_PATH/config
    echo 0x0C2A1008 > $DCC_PATH/config
    echo 0x0C2A100C > $DCC_PATH/config
    echo 0x0C2A1010 > $DCC_PATH/config
    echo 0x0C2A1014 > $DCC_PATH/config
    echo 0x0C2A1018 > $DCC_PATH/config
    echo 0x0C2A101C > $DCC_PATH/config
    echo 0x0C2A1020 > $DCC_PATH/config
    echo 0x0C2A1024 > $DCC_PATH/config
    echo 0x0C2A1028 > $DCC_PATH/config
    echo 0x0C2A102C > $DCC_PATH/config
    echo 0x0C2A1030 > $DCC_PATH/config
    echo 0x0C2A2260 > $DCC_PATH/config
    echo 0x0C2A2264 > $DCC_PATH/config
    echo 0x0C2A3008 > $DCC_PATH/config
    echo 0x0C2A300C > $DCC_PATH/config
    echo 0x0C2A3010 > $DCC_PATH/config
    echo 0x0C2A3014 > $DCC_PATH/config
    echo 0x0C2A3024 > $DCC_PATH/config
    echo 0x0C2A2034 > $DCC_PATH/config
    echo 0x0C2A214C > $DCC_PATH/config
    echo 0x0C2A2150 > $DCC_PATH/config
    echo 0x0C2A2154 > $DCC_PATH/config
}

config_atoll_dcc_apps_regs()
{
    #GOLD

}

config_atoll_dcc_bcm_seq_hang()
{
    echo 0x28206C 1 > $DCC_PATH/config
}

config_atoll_dcc_pll()
{
    echo 0x18282004 1 > $DCC_PATH/config
    echo 0x18325F6C 1 > $DCC_PATH/config
    echo 0x1808012C 1 > $DCC_PATH/config
    echo 0x1832582C 1 > $DCC_PATH/config
    echo 0x18280004 1 > $DCC_PATH/config
    echo 0x18284038 1 > $DCC_PATH/config
    echo 0x18284000 2 > $DCC_PATH/config
}

config_atoll_dcc_tsens_regs()
{
    echo 0x0C2630A0 4 > $DCC_PATH/config
    echo 0x0C2630B0 4 > $DCC_PATH/config
    echo 0x0C2630C0 4 > $DCC_PATH/config
    echo 0x0C2630D0 4 > $DCC_PATH/config
}

config_atoll_dcc_gpu()
{
    #GCC
    echo 0x105050 > $DCC_PATH/config
    echo 0x171004 > $DCC_PATH/config
    echo 0x171154 > $DCC_PATH/config
    echo 0x17100C > $DCC_PATH/config
    echo 0x171018 > $DCC_PATH/config

    #GPUCC
    echo 0x5091004 > $DCC_PATH/config
    echo 0x509100c > $DCC_PATH/config
    echo 0x5091010 > $DCC_PATH/config
    echo 0x5091014 > $DCC_PATH/config
    echo 0x5091054 > $DCC_PATH/config
    echo 0x5091060 > $DCC_PATH/config
    echo 0x509106c > $DCC_PATH/config
    echo 0x5091070 > $DCC_PATH/config
    echo 0x5091074 > $DCC_PATH/config
    echo 0x5091078 > $DCC_PATH/config
    echo 0x509107c > $DCC_PATH/config
    echo 0x509108c > $DCC_PATH/config
    echo 0x5091098 > $DCC_PATH/config
    echo 0x509109c > $DCC_PATH/config
}

config_atoll_dcc_core_hang(){
    echo 0x1800005C 1 > $DCC_PATH/config
    echo 0x1801005C 1 > $DCC_PATH/config
    echo 0x1802005C 1 > $DCC_PATH/config
    echo 0x1803005C 1 > $DCC_PATH/config
    echo 0x1804005C 1 > $DCC_PATH/config
    echo 0x1805005C 1 > $DCC_PATH/config
    echo 0x1806005C 1 > $DCC_PATH/config
    echo 0x1807005C 1 > $DCC_PATH/config
    echo 0x17C0003C 1 > $DCC_PATH/config
}

config_atoll_dcc_gic(){
    echo 0x17A00104 29 > $DCC_PATH/config
    echo 0x17A00204 29 > $DCC_PATH/config
}

config_atoll_dcc_rscc_npu(){
    echo 0x98B0010 3 > $DCC_PATH/config
    echo 0x98B0210 > $DCC_PATH/config
    echo 0x98B0230 > $DCC_PATH/config
    echo 0x98B0250 > $DCC_PATH/config
    echo 0x98B0270 > $DCC_PATH/config
    echo 0x98B0290 > $DCC_PATH/config
    echo 0x98B02B0 > $DCC_PATH/config
    echo 0x98B0208 > $DCC_PATH/config
    echo 0x98B0228 > $DCC_PATH/config
    echo 0x98B0248 > $DCC_PATH/config
    echo 0x98B0268 > $DCC_PATH/config
    echo 0x98B0288 > $DCC_PATH/config
    echo 0x98B02A8 > $DCC_PATH/config
    echo 0x98B020C > $DCC_PATH/config
    echo 0x98B022C > $DCC_PATH/config
    echo 0x98B024C > $DCC_PATH/config
    echo 0x98B026C > $DCC_PATH/config
    echo 0x98B028C > $DCC_PATH/config
    echo 0x98B02AC > $DCC_PATH/config
    echo 0x98B0400 3 > $DCC_PATH/config
    echo 0x9802028 > $DCC_PATH/config
    echo 0x9800304 > $DCC_PATH/config
}

config_atoll_lpass_debug(){
    #LPASS_Q6_MISC
    echo 0x62780054 > $DCC_PATH/config
    #LPASS_CBC_MISC
    echo 0x62780058 > $DCC_PATH/config
    #LPASS_LPASS_AON_CC_MISC
    echo 0x62780064 > $DCC_PATH/config
    #LPASS_MAIN_RCG_CFG_RCGR
    echo 0x62781004 > $DCC_PATH/config
    #LPASS_Q6_XO_CFG_RCGR
    echo 0x62788008 > $DCC_PATH/config
    #LPASS_QDSP6SS_CORE_CFG_RCGR
    echo 0x6240002C > $DCC_PATH/config
    #LPASS_QDSP6SS_CLK_STATUS
    echo 0x62400468 > $DCC_PATH/config
    #LPASS_QDSP6SS_CLK_CFG
    echo 0x62400460 > $DCC_PATH/config
    #LPASS_QDSP6SS_INTFCLAMP_STATUS
    echo 0x62400098 > $DCC_PATH/config
    #LPASS_QDSP6SS_CORE_CBCR
    echo 0x62400020 > $DCC_PATH/config
    #LPASS_QDSP6SS_SLEEP_CBCR
    echo 0x6240003C > $DCC_PATH/config
    #LPASS_QDSP6SS_XO_CBCR
    echo 0x62400038 > $DCC_PATH/config
    #LPASS_Q6_AHBM_CBCR
    echo 0x6278901C > $DCC_PATH/config
    #LPASS_Q6_AHBS_CBCR
    echo 0x62789020 > $DCC_PATH/config
    #LPASS_Q6_XO_CBCR
    echo 0x6278801C > $DCC_PATH/config
    #LPASS_RO_CBCR
    echo 0x6279000C > $DCC_PATH/config

    #LPASS_PLL_MODE
    echo 0x62780000 > $DCC_PATH/config
    #LPASS_PLL_L_VAL
    echo 0x62780004 > $DCC_PATH/config
    #LPASS_PLL_USER_CTL
    echo 0x6278000C > $DCC_PATH/config
    #LPASS_PLL_CONFIG_CTL
    echo 0x62780014 > $DCC_PATH/config

    #LPASS_PLL_STATUS
    echo 0x62780024 > $DCC_PATH/config
    #LPASS_PLL_OPMODE
    echo 0x6278002C > $DCC_PATH/config
    #LPASS_PLL_STATE
    echo 0x62780030 > $DCC_PATH/config
    #LPASS_USER_CTL_U
    echo 0x62780010 > $DCC_PATH/config
    #LPASS_CONFIG_CTL_U
    echo 0x62780018 > $DCC_PATH/config
}

config_atoll_dcc_async_package(){
    echo 0x06004FB0 0xc5acce55 > $DCC_PATH/config_write
    echo 0x0600408c 0xff > $DCC_PATH/config_write
    echo 0x06004FB0 0x0 > $DCC_PATH/config_write
}

# Function atoll DCC configuration
enable_atoll_dcc_config()
{
    DCC_PATH="/sys/bus/platform/devices/10a2000.dcc_v2"
    soc_version=`cat /sys/devices/soc0/revision`
    soc_version=${soc_version/./}

    if [ ! -d $DCC_PATH ]; then
        echo "DCC does not exist on this build."
        return
    fi

    echo 0 > $DCC_PATH/enable
    echo 1 > $DCC_PATH/config_reset
    echo 4 > $DCC_PATH/curr_list
    echo cap > $DCC_PATH/func_type
    echo sram > $DCC_PATH/data_sink
    config_atoll_dcc_lpm
    config_atoll_dcc_core_hang
    config_atoll_dcc_osm
    config_atoll_dcc_gemnoc
    config_atoll_dcc_noc_err_regs
    config_atoll_dcc_shrm
    config_atoll_dcc_cabo_llcc_shrm
    #config_atoll_dcc_memnoc_mccc
    config_atoll_dcc_cx_mx
    config_atoll_dcc_gcc_regs
    config_atoll_dcc_cprh
    config_atoll_dcc_bcm_seq_hang
    config_atoll_dcc_pll
    config_atoll_dcc_ddr
    config_atoll_dcc_tsens_regs
    config_atoll_dcc_rscc_apps
    config_atoll_dcc_gpu
    config_atoll_dcc_rscc_npu
    config_atoll_dcc_rscc_lpass
    config_atoll_dcc_rscc_modem
    config_atoll_dcc_rscc_cdsp
    config_atoll_lpass_debug
    echo 1 > /sys/bus/coresight/devices/coresight-tpdm-dcc/enable_source
    echo 3 > $DCC_PATH/curr_list
    echo cap > $DCC_PATH/func_type
    echo atb > $DCC_PATH/data_sink
    config_atoll_dcc_async_package
    config_atoll_dcc_gic
    #config_atoll_dcc_axi_pc
    #config_atoll_dcc_apb_pc
    #config_atoll_dcc_pdc_display
    #config_atoll_dcc_aop_rpmh
    #config_atoll_dcc_lmh
    #config_atoll_dcc_ipm_apps
    #config_atoll_dcc_ecc_llc
    #config_atoll_dcc_apps_regs
    #Enable below function with relaxed AC
    #config_atoll_regs_no_ac
    #Apply configuration and enable DCC

    echo  1 > $DCC_PATH/enable
}

enable_atoll_stm_hw_events()
{
   #TODO: Add HW events
}

enable_atoll_debug()
{
    echo "atoll debug"
    srcenable="enable_source"
    sinkenable="enable_sink"
    echo "Enabling STM events on atoll."
    enable_atoll_stm_events
    if [ "$ftrace_disable" != "Yes" ]; then
        enable_atoll_ftrace_event_tracing
    fi
    enable_atoll_dcc_config
    enable_atoll_stm_hw_events
}
