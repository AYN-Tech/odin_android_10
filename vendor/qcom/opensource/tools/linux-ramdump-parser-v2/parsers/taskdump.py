# Copyright (c) 2012-2013, 2015, 2017-2020 The Linux Foundation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 and
# only version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import string
from print_out import print_out_str
from parser_util import register_parser, RamParser, cleanupString
taskhighlight_out = None
highlight_tasks = "\n=====List of all runing and uninterruptable sleep process====\n"
import ctypes

def find_panic(ramdump, addr_stack, thread_task_name):
    if ramdump.arm64:
        stack_size = 0x4000
        increment = 8
    else:
        stack_size = 0x2000
        increment = 4
    for i in range(addr_stack, addr_stack + stack_size, increment):
        if ramdump.arm64:
            pc = ramdump.read_word(i + 8) - 4
            fp = ramdump.read_word(i)
            spx = i + 16
            lr = 0
        else:
            pc = ramdump.read_word(i)
            lr = ramdump.read_word(i + 4)
            spx = i + 4
            fp = 0
        l = ramdump.unwind_lookup(pc)
        if l is not None:
            s, offset = l
            if s == 'panic':
                print_out_str('Faulting process found! Name {0})'.format(thread_task_name))
                ramdump.unwind.unwind_backtrace(spx, fp, pc, lr, '')
                regspanic = ramdump.open_file('regs_panic.cmm')
                if ramdump.arm64:
                    regspanic.write('r.s pc 0x{0:x}\n'.format(pc))
                    regspanic.write('r.s sp 0x{0:x}\n'.format(spx))
                else:
                    regspanic.write('r.s pc 0x{0:x}\n'.format(pc))
                    regspanic.write('r.s r13 0x{0:x}\n'.format(i + 4))
                regspanic.close()
                return True
    return False

#Definitions taken from include/linux/sched.h
TASK_REPORT = 0xff
task_state_short_name = "RSDTtXZPI"

def find_last_bit_index(count):
    count &= TASK_REPORT
    index = count.bit_length()
    return index

def task_state_to_char(task_state):
    index = find_last_bit_index(task_state)
    task_state_str = task_state_short_name[index]
    return task_state_str

def dump_thread_group(ramdump, thread_group, task_out, taskhighlight_out, check_for_panic=0):
    global highlight_tasks
    offset_thread_group = ramdump.field_offset(
        'struct task_struct', 'thread_group')
    offset_comm = ramdump.field_offset('struct task_struct', 'comm')
    offset_pid = ramdump.field_offset('struct task_struct', 'pid')
    offset_stack = ramdump.field_offset('struct task_struct', 'stack')
    offset_state = ramdump.field_offset('struct task_struct', 'state')
    offset_prio = ramdump.field_offset('struct task_struct', 'prio')
    offset_last_enqueued_ts = ramdump.field_offset('struct task_struct', 'last_enqueued_ts')
    offset_last_sleep_ts = ramdump.field_offset('struct task_struct', 'last_sleep_ts')
    if ramdump.kernel_version > (5, 2, 0):
        offset_affine = ramdump.field_offset('struct task_struct', 'cpus_mask')
    else:
        offset_affine = ramdump.field_offset('struct task_struct', 'cpus_allowed')

    offset_exit_state = ramdump.field_offset(
        'struct task_struct', 'exit_state')
    orig_thread_group = thread_group
    first = 0
    seen_threads = []
    while True:
        next_thread_start = thread_group - offset_thread_group
        next_thread_comm = next_thread_start + offset_comm
        next_thread_pid = next_thread_start + offset_pid
        next_thread_prio = next_thread_start + offset_prio
        next_thread_stack = next_thread_start + offset_stack
        next_thread_state = next_thread_start + offset_state
        next_thread_exit_state = next_thread_start + offset_exit_state
        next_thread_affine = next_thread_start + offset_affine
        next_thread_info = ramdump.get_thread_info_addr(next_thread_start)
        thread_task_name = cleanupString(
            ramdump.read_cstring(next_thread_comm, 16))
        if thread_task_name is None or thread_task_name == "":
            return
        thread_task_prio = ramdump.read_int(next_thread_prio)
        if thread_task_prio is None:
            return
        # Task prio is an integer and it can be -1 for DL tasks.
        thread_task_prio = ctypes.c_int(thread_task_prio).value
        thread_task_pid = ramdump.read_int(next_thread_pid)
        if thread_task_pid is None:
            return
        thread_task_affine = ramdump.read_u64(next_thread_affine)
        if thread_task_affine is None:
            return
        task_state = ramdump.read_word(next_thread_state)
        if task_state is None:
            return
        task_state_str = task_state_to_char(task_state)

        task_exit_state = ramdump.read_int(next_thread_exit_state)
        if task_exit_state is None:
            return
        addr_stack = ramdump.read_word(next_thread_stack)
        if addr_stack is None:
            return
        threadinfo = next_thread_info
        if threadinfo is None:
            return

        if offset_last_enqueued_ts is None:
            task_last_enqueued_ts = 0
        else:
            next_thread_last_enqueued_ts = next_thread_start + offset_last_enqueued_ts
            task_last_enqueued_ts = ramdump.read_u64(next_thread_last_enqueued_ts)
            if task_last_enqueued_ts is None:
                task_last_enqueued_ts = 0
        if offset_last_sleep_ts is None:
            task_last_sleep_ts = 0
        else:
            next_thread_last_sleep_ts = next_thread_start + offset_last_sleep_ts
            task_last_sleep_ts = ramdump.read_u64(next_thread_last_sleep_ts)
            if task_last_sleep_ts is None:
                task_last_sleep_ts = 0

        if not check_for_panic:
            task_cpu = ramdump.get_task_cpu(next_thread_start, threadinfo)
            #thread_line = thread_task_pid + task_cpu + task_state_str+ next_thread_start+thread_task_name
            thread_line = "PID %6d cpu %1d  state %16s hex 0x%06x start 0x%x comm %32s\n" %(thread_task_pid, task_cpu,
                task_state_str, task_state, next_thread_start, thread_task_name)
            if task_state != 1:
                if not first:
                    highlight_tasks += "*" + thread_line
                    taskhighlight_out.write("**"+thread_line)
                else:
                    highlight_tasks += " " + thread_line
                    taskhighlight_out.write("  " + thread_line)
                ramdump.unwind.unwind_backtrace(
                     ramdump.thread_saved_sp(next_thread_start),
                     ramdump.thread_saved_fp(next_thread_start),
                     ramdump.thread_saved_pc(next_thread_start),
                     0, '    ', taskhighlight_out)
                thread_line = '+' + thread_line
            else:
                thread_line = ' ' + thread_line

            if not first:
                task_out.write('Process: {0}, [affinity: 0x{1:x}] cpu: {2} pid: {3} start: 0x{4:x}\n'.format(
                    thread_task_name, thread_task_affine, task_cpu, thread_task_pid, next_thread_start))
                task_out.write(
                    '=====================================================\n')
                first = 1
            task_out.write('    Task name: {0} pid: {1} cpu: {2} prio: {7} start: {'
                           '6:x}\n    state: 0x{3:x}[{8}] exit_state: 0x{4:x}'
                           ' stack base: 0x{5:x}\n'
                           '    Last_enqueued_ts:{9:18.9f} Last_sleep_ts:{10:18.9f}\n'.format(
                thread_task_name, thread_task_pid, task_cpu, task_state,
                task_exit_state, addr_stack, next_thread_start, thread_task_prio, task_state_str,
                task_last_enqueued_ts/1000000000.0, task_last_sleep_ts/1000000000.0))
            task_out.write('    Stack:\n')
            ramdump.unwind.unwind_backtrace(
                 ramdump.thread_saved_sp(next_thread_start),
                 ramdump.thread_saved_fp(next_thread_start),
                 ramdump.thread_saved_pc(next_thread_start),
                 0, '    ', task_out)
            task_out.write(
                '=======================================================\n')
        # Panicking tasks are expected to remain in a TASK_RUNNING state
        elif task_state == 0:
            find_panic(ramdump, addr_stack, thread_task_name)

        next_thr = ramdump.read_word(thread_group)
        if (next_thr == thread_group) and (next_thr != orig_thread_group):
            if not check_for_panic:
                task_out.write(
                    '!!!! Cycle in thread group! The list is corrupt!\n')
            break
        if (next_thr in seen_threads):
            break

        seen_threads.append(next_thr)
        thread_group = next_thr
        if thread_group == orig_thread_group:
            break


def do_dump_stacks(ramdump, check_for_panic=0):
    offset_tasks = ramdump.field_offset('struct task_struct', 'tasks')
    prev_offset = ramdump.field_offset('struct list_head','prev')
    offset_thread_group = ramdump.field_offset(
        'struct task_struct', 'thread_group')
    init_addr = ramdump.address_of('init_task')
    init_next_task = init_addr + offset_tasks
    orig_init_next_task = init_next_task
    init_thread_group = init_addr + offset_thread_group
    seen_tasks = []
    if check_for_panic == 0:
        task_out = ramdump.open_file('tasks.txt')
        taskhighlight_out = ramdump.open_file('tasks_highlight.txt')
    else:
        task_out = None
        taskhighlight_out = None
    while True:
        dump_thread_group(ramdump, init_thread_group,
                          task_out, taskhighlight_out, check_for_panic)
        next_task = ramdump.read_word(init_next_task)
        if next_task is None:
            init_next_task = init_addr + offset_tasks
            init_next_task = init_next_task + prev_offset
            init_next_task = ramdump.read_word(init_next_task)
            init_thread_group = init_next_task - offset_tasks \
                                + offset_thread_group
            while True:
                dump_thread_group(ramdump, init_thread_group,
                                  task_out, taskhighlight_out, check_for_panic)
                init_next_task = init_next_task + prev_offset
                orig_init_next_task = init_next_task
                next_task = ramdump.read_word(init_next_task)
                if next_task is None:
                    break

                if (next_task == init_next_task) and (next_task !=
                                    orig_init_next_task):
                    if not check_for_panic:
                        task_out.write(
                            '!!! Cycle in task list! the list is corrupt!\n')
                        break
                if (next_task in seen_tasks):
                    break

                seen_tasks.append(next_task)
                init_next_task = next_task
                init_thread_group = init_next_task - offset_tasks\
                                    + offset_thread_group
                if init_next_task == orig_init_next_task:
                    break

            task_out.write('\n\n!!!Some task might be missing in task.txt')
            break

        if (next_task == init_next_task) and (next_task != orig_init_next_task):
            if not check_for_panic:
                task_out.write(
                    '!!!! Cycle in task list! The list is corrupt!\n')
            break

        if (next_task in seen_tasks):
            break

        seen_tasks.append(next_task)

        init_next_task = next_task
        init_thread_group = init_next_task - offset_tasks + offset_thread_group
        if init_next_task == orig_init_next_task:
            break
    if check_for_panic == 0:
        task_out.close()
        taskhighlight_out.write(highlight_tasks + "\n")
        taskhighlight_out.close()
        print_out_str('---wrote tasks to tasks.txt')

def do_dump_task_timestamps(ramdump):
    offset_tasks = ramdump.field_offset('struct task_struct', 'tasks')
    offset_comm = ramdump.field_offset('struct task_struct', 'comm')
    offset_thread_group = ramdump.field_offset(
        'struct task_struct', 'thread_group')
    offset_pid = ramdump.field_offset('struct task_struct', 'pid')
    init_addr = ramdump.address_of('init_task')
    prev_offset = ramdump.field_offset('struct list_head','prev')
    init_next_task = init_addr + offset_tasks
    orig_init_next_task = init_next_task
    init_thread_group = init_addr + offset_thread_group
    count = 0
    seen_tasks = []
    task_out = []
    no_of_cpus = ramdump.get_num_cpus()
    t = [[] for j in range(no_of_cpus)]
    for i in range(0, no_of_cpus):
        task_file = ramdump.open_file('tasks_sched_stats{0}.txt'.format(i))
        task_out.append(task_file)
    while True:
        ret = dump_thread_group_timestamps(ramdump, init_thread_group,t)
        if ret is False:
            count = 1
        next_task = ramdump.read_word(init_next_task)
        if next_task is None:
            init_next_task = init_addr + offset_tasks
            init_next_task = init_next_task + prev_offset
            init_next_task = ramdump.read_word(init_next_task)
            init_thread_group = init_next_task - offset_tasks \
                                + offset_thread_group
            while True:
                ret = dump_thread_group_timestamps(ramdump,
                                init_thread_group, t)
                if ret is False:
                    count = 1

                init_next_task = init_next_task + prev_offset
                orig_init_next_task = init_next_task
                next_task = ramdump.read_word(init_next_task)
                next_task = ramdump.read_word(init_next_task)
                if next_task is None:
                    break
                if (next_task == init_next_task) and (
                    next_task != orig_init_next_task):
                    break

                if (next_task in seen_tasks):
                    break

                seen_tasks.append(next_task)

                init_next_task = next_task
                init_thread_group = init_next_task - offset_tasks + offset_thread_group
                if init_next_task == orig_init_next_task:
                    break
            break

        if (next_task == init_next_task) and (next_task != orig_init_next_task):
            break

        if (next_task in seen_tasks):
            break

        seen_tasks.append(next_task)

        init_next_task = next_task
        init_thread_group = init_next_task - offset_tasks + offset_thread_group
        if init_next_task == orig_init_next_task:
            break
    for i in range(0, no_of_cpus):
        if count == 1:
            task_out[i].write('!!!Note : Some thread may be missing\n\n')
        t[i] = sorted(t[i], key=lambda l:l[2], reverse=True)
        str = '{0:<17s}{1:>8s}{2:>18s}{3:>18s}{4:>18s}{5:>17s}' \
              ' {6:>8s}{7:>8s}{8:>18s}{9:>18s}\n'.format(
                    'Task name', 'PID', 'Exec_Started_at',
                    'Last_Queued_at', 'Total_wait_time',
                    'No_of_times_exec', 'Prio', 'State',
                    'Last_enqueued_ts', 'Last_sleep_ts')
        task_out[i].write(str)
        for item in t[i]:
            str = '{0:<17s}{1:8d}{2:18.9f}{3:18.9f}{4:18.9f}{5:17d}{6:8d}{7:>9s}{8:18.9f}{9:18.9f}\n'\
                    .format(
                        item[0], item[1], item[2]/1000000000.0,
                        item[3]/1000000000.0, item[4]/1000000000.0,
                        item[5], item[6], item[7], item[8]/1000000000.0,
                        item[9]/1000000000.0)
            task_out[i].write(str)
        task_out[i].close()
        print_out_str('---wrote tasks to tasks_sched_stats{0}.txt'.format(i))

def dump_thread_group_timestamps(ramdump, thread_group, t):
    offset_thread_group = ramdump.field_offset(
        'struct task_struct', 'thread_group')
    offset_comm = ramdump.field_offset('struct task_struct', 'comm')
    offset_pid = ramdump.field_offset('struct task_struct', 'pid')
    offset_task = ramdump.field_offset('struct thread_info', 'task')
    offset_stack = ramdump.field_offset('struct task_struct', 'stack')
    offset_prio = ramdump.field_offset('struct task_struct', 'prio')
    offset_schedinfo = ramdump.field_offset('struct task_struct', 'sched_info')
    offset_last_arrival = offset_schedinfo + ramdump.field_offset('struct sched_info', 'last_arrival')
    offset_last_queued = offset_schedinfo + ramdump.field_offset('struct sched_info', 'last_queued')
    offset_last_pcount = offset_schedinfo + ramdump.field_offset('struct sched_info', 'pcount')
    offset_last_rundelay = offset_schedinfo + ramdump.field_offset('struct sched_info', 'run_delay')
    offset_state = ramdump.field_offset('struct task_struct', 'state')
    offset_last_enqueued_ts = ramdump.field_offset('struct task_struct', 'last_enqueued_ts')
    offset_last_sleep_ts = ramdump.field_offset('struct task_struct', 'last_sleep_ts')
    orig_thread_group = thread_group
    first = 0
    seen_threads = []

    while True:
        next_thread_start = thread_group - offset_thread_group
        next_thread_comm = next_thread_start + offset_comm
        next_thread_pid = next_thread_start + offset_pid
        next_thread_prio = next_thread_start + offset_prio
        next_thread_last_arrival = next_thread_start + offset_last_arrival
        next_thread_last_queued = next_thread_start + offset_last_queued
        next_thread_pcount = next_thread_start + offset_last_pcount
        next_thread_run_delay = next_thread_start + offset_last_rundelay
        next_thread_stack = next_thread_start + offset_stack
        next_thread_info = ramdump.get_thread_info_addr(next_thread_start)
        next_thread_state = next_thread_start + offset_state

        if offset_last_enqueued_ts is None:
            thread_last_enqueued_ts = 0
        if offset_last_enqueued_ts is not None:
            next_thread_last_enqueued_ts = next_thread_start + offset_last_enqueued_ts
            thread_last_enqueued_ts = ramdump.read_u64(next_thread_last_enqueued_ts)
            if thread_last_enqueued_ts is None:
                thread_last_enqueued_ts = 0

        if offset_last_sleep_ts is None:
            thread_last_sleep_ts = 0
        else:
            next_thread_last_sleep_ts = next_thread_start + offset_last_sleep_ts
            thread_last_sleep_ts = ramdump.read_u64(next_thread_last_sleep_ts)
            if thread_last_sleep_ts is None:
                thread_last_sleep_ts = 0

        addr_stack = ramdump.read_word(next_thread_stack)
        if addr_stack is None:
            print_out_str('!!!! Task list corruption\n')
            return False
        threadinfo = next_thread_info
        thread_task_name = cleanupString(
            ramdump.read_cstring(next_thread_comm, 16))
        thread_task_pid = ramdump.read_int(next_thread_pid)
        thread_task_prio = ramdump.read_int(next_thread_prio)
        if thread_task_prio is None:
            return False
        thread_task_state = ramdump.read_int(next_thread_state)
        if thread_task_state is None:
            return False
        thread_task_state_str = task_state_to_char(thread_task_state)
        cpu_no = ramdump.get_task_cpu(next_thread_start, threadinfo)
        if cpu_no >= ramdump.get_num_cpus():
            return False
        if not ramdump.is_thread_info_in_task():
            thread_info_task = ramdump.read_word(threadinfo + offset_task)
            if next_thread_start != thread_info_task:
                print_out_str('!!!! Task list or Thread info corruption\n{0}  {1}'.format(next_thread_start,thread_info_task))
                return False
        t[cpu_no].append([thread_task_name, thread_task_pid,
            ramdump.read_u64(next_thread_last_arrival),
            ramdump.read_u64(next_thread_last_queued),
            ramdump.read_u64(next_thread_run_delay),
            ramdump.read_word(next_thread_pcount),
            thread_task_prio,thread_task_state_str,
            thread_last_enqueued_ts,
            thread_last_sleep_ts])
        next_thr = ramdump.read_word(thread_group)
        if (next_thr == thread_group) and (next_thr != orig_thread_group):
            print_out_str('!!!! Cycle in thread group! The list is corrupt!\n')
            return False
        if (next_thr in seen_threads):
            break

        seen_threads.append(next_thr)
        thread_group = next_thr
        if thread_group == orig_thread_group:
            break
    return True

@register_parser('--print-tasks', 'Print all the task information', shortopt='-t')
class DumpTasks(RamParser):

    def parse(self):
        do_dump_stacks(self.ramdump, 0)

@register_parser('--print-tasks-timestamps', 'Print all the task sched stats per core sorted on arrival time', shortopt='-T')
class DumpTasksTimeStamps(RamParser):

    def parse(self):
        do_dump_task_timestamps(self.ramdump)

@register_parser('--check-for-panic', 'Check if a kernel panic occured', shortopt='-p')
class CheckForPanic(RamParser):

    def parse(self):
        addr = self.ramdump.address_of('in_panic')

        result = self.ramdump.read_word(addr)

        if result == 1:
            print_out_str('-------------------------------------------------')
            print_out_str('[!] KERNEL PANIC detected!')
            print_out_str('-------------------------------------------------')
            do_dump_stacks(self.ramdump, 1)
        else:
            print_out_str('No kernel panic detected')
