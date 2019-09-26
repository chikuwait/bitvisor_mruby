/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "asm.h"
#include "config.h"
#include "constants.h"
#include "convert.h"
#include "cpu_emul.h"
#include "cpu_mmu.h"
#include "current.h"
#include "exint_pass.h"
#include "gmm_pass.h"
#include "initfunc.h"
#include "initipi.h"
#include "int.h"
#include "linkage.h"
#include "nmi.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "reboot.h"
#include "string.h"
#include "thread.h"
#include "vmmcall.h"
#include "vmmcall_status.h"
#include "mm.h"
#include "vt.h"
#include "vt_addip.h"
#include "vt_exitreason.h"
#include "vt_init.h"
#include "vt_io.h"
#include "vt_main.h"
#include "vt_paging.h"
#include "vt_regs.h"
#include "vt_shadow_vt.h"
#include "vt_vmcs.h"

#define EPT_VIOLATION_EXIT_QUAL_WRITE_BIT 0x2
#define STAT_EXIT_REASON_MAX EXIT_REASON_XSETBV
enum vt__status {
	VT__VMENTRY_SUCCESS,
	VT__VMENTRY_FAILED,
	VT__VMEXIT,
	VT__NMI,
};

static u32 stat_intcnt = 0;
static u32 stat_hwexcnt = 0;
static u32 stat_swexcnt = 0;
static u32 stat_pfcnt = 0;
static u32 stat_iocnt = 0;
static u32 stat_hltcnt = 0;
static u32 stat_exit_reason[STAT_EXIT_REASON_MAX + 1];

static inline void current_thread_info(void);

static void
do_mov_cr (void)
{
	ulong val;
	union {
		struct exit_qual_cr s;
		ulong v;
	} eqc;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqc.v);
	switch (eqc.s.type) {
	case EXIT_QUAL_CR_TYPE_MOV_TO_CR:
		vt_read_general_reg (eqc.s.reg, &val);
		vt_write_control_reg (eqc.s.num, val);
		break;
	case EXIT_QUAL_CR_TYPE_MOV_FROM_CR:
		vt_read_control_reg (eqc.s.num, &val);
		vt_write_general_reg (eqc.s.reg, val);
		break;
	case EXIT_QUAL_CR_TYPE_CLTS:
		vt_read_control_reg (CONTROL_REG_CR0, &val);
		val &= ~CR0_TS_BIT;
		vt_write_control_reg (CONTROL_REG_CR0, val);
		break;
	case EXIT_QUAL_CR_TYPE_LMSW:
		vt_read_control_reg (CONTROL_REG_CR0, &val);
		val &= ~0xFFFF;
		val |= eqc.s.lmsw_src;
		vt_write_control_reg (CONTROL_REG_CR0, val);
		break;
	default:
		panic ("Fatal error: Not implemented.");
	}
	add_ip ();
}

static void
do_cpuid (void)
{
	cpu_emul_cpuid ();
	add_ip ();
}

static void
make_gp_fault (u32 errcode)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	vid->vmcs_intr_info.s.vector = EXCEPTION_GP;
	vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_HARD_EXCEPTION;
	vid->vmcs_intr_info.s.err = INTR_INFO_ERR_VALID;
	vid->vmcs_intr_info.s.nmi = 0;
	vid->vmcs_intr_info.s.reserved = 0;
	vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
	vid->vmcs_exception_errcode = errcode;
	vid->vmcs_instruction_len = 0;
}

static void
make_ud_fault (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	vid->vmcs_intr_info.s.vector = EXCEPTION_UD;
	vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_HARD_EXCEPTION;
	vid->vmcs_intr_info.s.err = INTR_INFO_ERR_INVALID;
	vid->vmcs_intr_info.s.nmi = 0;
	vid->vmcs_intr_info.s.reserved = 0;
	vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
	vid->vmcs_exception_errcode = 0;
	vid->vmcs_instruction_len = 0;
}

static void
do_rdmsr (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;
	u32 v;

	v = vid->vmcs_intr_info.v;
	if (cpu_emul_rdmsr ()) {
		if (v == vid->vmcs_intr_info.v) /* not page fault */
			make_gp_fault (0);
	} else
		add_ip ();
}

static void
do_wrmsr (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;
	u32 v;

	v = vid->vmcs_intr_info.v;
	if (cpu_emul_wrmsr ()) {
		if (v == vid->vmcs_intr_info.v) /* not page fault */
			make_gp_fault (0);
	} else
		add_ip ();
}

void
vt_update_exception_bmp (void)
{
	u32 newbmp = 0xFFFFFFFF;

	if (!current->u.vt.vr.re && !current->u.vt.vr.sw.enable) {
		newbmp = 1 << EXCEPTION_NMI;
		if (current->u.vt.handle_pagefault)
			newbmp |= 1 << EXCEPTION_PF;
	}
	asm_vmwrite (VMCS_EXCEPTION_BMP, newbmp);
}

static void
vt_generate_nmi (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (current->u.vt.vr.re)
		panic ("NMI in real mode");
	vid->vmcs_intr_info.v = 0;
	vid->vmcs_intr_info.s.vector = EXCEPTION_NMI;
	vid->vmcs_intr_info.s.type = INTR_INFO_TYPE_NMI;
	vid->vmcs_intr_info.s.err = INTR_INFO_ERR_INVALID;
	vid->vmcs_intr_info.s.valid = INTR_INFO_VALID_VALID;
	vid->vmcs_instruction_len = 0;
}

/* Generate an NMI in the VM */
static void
vt_nmi_has_come (void)
{
	ulong is, proc_based_vmexec_ctl;
	struct vt_intr_data *vid = &current->u.vt.intr;

	/* If the vmcs_intr_info is set to NMI, NMI will be generated
	 * soon. */
	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID &&
	    vid->vmcs_intr_info.s.type == INTR_INFO_TYPE_NMI)
		return;
	/* If NMI-window exiting bit is set, VM Exit reason "NMI
	   window" will generate NMI. */
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &proc_based_vmexec_ctl);
	if (proc_based_vmexec_ctl & VMCS_PROC_BASED_VMEXEC_CTL_NMIWINEXIT_BIT)
		return;
	/* If blocking by STI bit and blocking by MOV SS bit and
	   blocking by NMI bit are not set and no injection exists,
	   generate NMI now. */
	asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
	if (!(is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_STI_BIT) &&
	    !(is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_MOV_SS_BIT) &&
	    !(is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_NMI_BIT) &&
	    vid->vmcs_intr_info.s.valid != INTR_INFO_VALID_VALID) {
		vt_generate_nmi ();
		return;
	}
	/* Use NMI-window exiting to get the correct timing to inject
	 * NMIs.  This is a workaround for a processor that makes a VM
	 * Entry failure when NMI is injected while blocking by STI
	 * bit is set.  If blocking by NMI bit is set, the next NMI
	 * will be generated after IRET which makes NMI window VM
	 * Exit. */
	proc_based_vmexec_ctl |= VMCS_PROC_BASED_VMEXEC_CTL_NMIWINEXIT_BIT;
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, proc_based_vmexec_ctl);
}

static void
do_exception (void)
{
	union {
		struct intr_info s;
		ulong v;
	} vii;
	ulong len;
	enum vmmerr err;
	ulong errc;

	asm_vmread (VMCS_VMEXIT_INTR_INFO, &vii.v);
	if (vii.s.valid == INTR_INFO_VALID_VALID) {
		switch (vii.s.type) {
		case INTR_INFO_TYPE_HARD_EXCEPTION:
			STATUS_UPDATE (asm_lock_incl (&stat_hwexcnt));
			if (vii.s.vector == EXCEPTION_DB &&
			    current->u.vt.vr.sw.enable)
				break;
			if (vii.s.vector == EXCEPTION_PF) {
				ulong err, cr2;

				asm_vmread (VMCS_VMEXIT_INTR_ERRCODE, &err);
				asm_vmread (VMCS_EXIT_QUALIFICATION, &cr2);
				vt_paging_pagefault (err, cr2);
				STATUS_UPDATE (asm_lock_incl (&stat_pfcnt));
			} else if (current->u.vt.vr.re) {
				switch (vii.s.vector) {
				case EXCEPTION_GP:
					err = cpu_interpreter ();
					if (err == VMMERR_SUCCESS)
						break;
					panic ("Fatal error:"
					       " General protection fault"
					       " in real mode."
					       " (err: %d)", err);
				case EXCEPTION_DB:
					if (cpu_emul_realmode_int (1)) {
						panic ("Fatal error:"
						       " realmode_int"
						       " error"
						       " (int 1)");
					}
					break;
				default:
					panic ("Fatal error:"
					       " Unimplemented vector 0x%X",
					       vii.s.vector);
				}
			} else {
				if (current->u.vt.intr.vmcs_intr_info.v
				    == vii.v)
					panic ("Double fault"
					       " in do_exception.");
				current->u.vt.intr.vmcs_intr_info.v = vii.v;
				if (vii.s.err == INTR_INFO_ERR_VALID) {
					asm_vmread (VMCS_VMEXIT_INTR_ERRCODE,
						    &errc);
					current->u.vt.intr.
						vmcs_exception_errcode = errc;
					if (vii.s.vector == EXCEPTION_GP &&
					    errc == 0x6B)
						panic ("Fatal error:"
						       " General protection"
						       " fault"
						       " (maybe double"
						       " fault).");
				}
				current->u.vt.intr.vmcs_instruction_len = 0;
#if 0				/* Exception monitoring test */
				if (vii.s.vector == EXCEPTION_DE) {
					u32 cs, eip;

					asm_vmread (VMCS_GUEST_CS_SEL, &cs);
					asm_vmread (VMCS_GUEST_RIP, &eip);
					printf ("Exception monitor test:"
						" Devide Error Exception"
						" at 0x%x:0x%x\n",
						cs, eip);
				}
#endif				/* Exception monitoring test */
			}
			break;
		case INTR_INFO_TYPE_SOFT_EXCEPTION:
			STATUS_UPDATE (asm_lock_incl (&stat_swexcnt));
			current->u.vt.intr.vmcs_intr_info.v = vii.v;
			asm_vmread (VMCS_VMEXIT_INSTRUCTION_LEN, &len);
			current->u.vt.intr.vmcs_instruction_len = len;
			break;
		case INTR_INFO_TYPE_NMI:
			nmi_inc_count ();
			break;
		case INTR_INFO_TYPE_EXTERNAL:
		default:
			panic ("Fatal error:"
			       " intr_info_type %d not implemented",
			       vii.s.type);
		}
	}
}

static void
do_invlpg (void)
{
	ulong linear;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &linear);
	vt_paging_invalidate (linear);
	add_ip ();
}

/* VMCALL: guest calls VMM */
static void
do_vmcall (void)
{
	add_ip ();
	vmmcall ();
}

static void
do_nmi_window (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;
	ulong proc_based_vmexec_ctl;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID) {
		/* This may be incorrect behavior... */
		printf ("Maskable interrupt and NMI at the same time\n");
		return;
	}
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &proc_based_vmexec_ctl);
	proc_based_vmexec_ctl &= ~VMCS_PROC_BASED_VMEXEC_CTL_NMIWINEXIT_BIT;
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, proc_based_vmexec_ctl);
	vt_generate_nmi ();
}

static void
vt__nmi (void)
{
	if (!current->nmi.get_nmi_count ())
		return;
	vt_nmi_has_come ();
}

static void
vt__event_delivery_setup (void)
{
	struct vt_intr_data *vid = &current->u.vt.intr;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID) {
		asm_vmwrite (VMCS_VMENTRY_INTR_INFO_FIELD,
			     vid->vmcs_intr_info.v);
		if (vid->vmcs_intr_info.s.err == INTR_INFO_ERR_VALID)
			asm_vmwrite (VMCS_VMENTRY_EXCEPTION_ERRCODE,
				     vid->vmcs_exception_errcode);
		asm_vmwrite (VMCS_VMENTRY_INSTRUCTION_LEN,
			     vid->vmcs_instruction_len);
	}
}

static enum vt__status
call_vt__vmlaunch (void)
{
	switch (asm_vmlaunch_regs (&current->u.vt.vr)) {
	case 0:
		return VT__VMEXIT;
	case 1:
		return VT__NMI;
	default:
		return VT__VMENTRY_FAILED;
	}
}

static enum vt__status
call_vt__vmresume (void)
{
	switch (asm_vmresume_regs (&current->u.vt.vr)) {
	case 0:
		return VT__VMEXIT;
	case 1:
		return VT__NMI;
	default:
		return VT__VMENTRY_FAILED;
	}
}

static bool
vt__vm_run_first (void)
{
	enum vt__status status;
	ulong errnum;

	status = call_vt__vmlaunch ();
	if (status == VT__NMI)
		return true;
	if (status != VT__VMEXIT) {
		asm_vmread (VMCS_VM_INSTRUCTION_ERR, &errnum);
		if (status == VT__VMENTRY_FAILED)
			panic ("Fatal error: VM entry failed. Error %lu",
			       errnum);
		else
			panic ("Fatal error: Strange status.");
	}
	return false;
}

static bool
vt__vm_run (void)
{
	enum vt__status status;
	ulong errnum;
	bool ret;

	if (current->u.vt.first) {
		ret = vt__vm_run_first ();
		if (!ret)
			current->u.vt.first = false;
		return ret;
	}
	if (current->u.vt.exint_update)
		vt_update_exint ();
	if (current->u.vt.saved_vmcs)
		spinlock_unlock (&currentcpu->suspend_lock);
	status = call_vt__vmresume ();
	if (current->u.vt.saved_vmcs)
		spinlock_lock (&currentcpu->suspend_lock);
	if (status == VT__NMI)
		return true;
	if (status != VT__VMEXIT) {
		asm_vmread (VMCS_VM_INSTRUCTION_ERR, &errnum);
		if (status == VT__VMENTRY_FAILED)
			panic ("Fatal error: VM entry failed. Error %lu",
			       errnum);
		else
			panic ("Fatal error: Strange status.");
	}
	return false;
}

/* FIXME: bad handling of TF bit */
static bool
vt__vm_run_with_tf (void)
{
	ulong rflags;
	bool ret;

	vt_read_flags (&rflags);
	rflags |= RFLAGS_TF_BIT;
	vt_write_flags (rflags);
	ret = vt__vm_run ();
	vt_read_flags (&rflags);
	rflags &= ~RFLAGS_TF_BIT;
	vt_write_flags (rflags);
	return ret;
}

static void
clear_blocking_by_nmi (void)
{
	ulong is;

	asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
	is &= ~VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_NMI_BIT;
	asm_vmwrite (VMCS_GUEST_INTERRUPTIBILITY_STATE, is);
}

static void
vt__event_delivery_check (void)
{
	ulong err;
	union {
		struct intr_info s;
		ulong v;
	} ivif;
	struct vt_intr_data *vid = &current->u.vt.intr;

	/* The IDT-vectoring information field has event information
	   that is not delivered yet. The event will be the next
	   event if no other events will have been injected.
	   This field may or may not be the same as the VM-entry
	   interruption-information field. Atom Z520/Z530 seems to
	   behave differently from other processors about this field. */
	asm_vmread (VMCS_IDT_VECTORING_INFO_FIELD, &ivif.v);
	if (ivif.s.valid == INTR_INFO_VALID_VALID) {
		if (ivif.s.type == INTR_INFO_TYPE_SOFT_INTR ||
		    ivif.s.type == INTR_INFO_TYPE_SOFT_EXCEPTION) {
			/* Ignore software interrupt to
			   make this function simple.
			   The INT instruction will be executed again. */
			ivif.s.valid = INTR_INFO_VALID_INVALID;
		} else if (ivif.s.err == INTR_INFO_ERR_VALID) {
			asm_vmread (VMCS_IDT_VECTORING_ERRCODE, &err);
			vid->vmcs_exception_errcode = err;
		} else if (ivif.s.type == INTR_INFO_TYPE_NMI) {
			/* If EPT violation happened during injecting
			 * NMI, blocking by NMI bit is set.  It must
			 * be cleared before injecting NMI again. */
			clear_blocking_by_nmi ();
		}
	}
	vid->vmcs_intr_info.v = ivif.v;
}

void
vt_init_signal (void)
{
	if (currentcpu->cpunum == 0)
		handle_init_to_bsp ();

	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE,
		     VMCS_GUEST_ACTIVITY_STATE_WAIT_FOR_SIPI);
	current->halt = false;
	current->u.vt.vr.sw.enable = 0;
	vt_update_exception_bmp ();
}

static void
do_init_signal (void)
{
	initipi_inc_count ();
}

static void
do_startup_ipi (void)
{
	ulong vector;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &vector);
	vector &= 0xFF;
	vt_reset ();
	vt_write_realmode_seg (SREG_CS, vector << 8);
	vt_write_general_reg (GENERAL_REG_RAX, 0);
	vt_write_general_reg (GENERAL_REG_RCX, 0);
	vt_write_general_reg (GENERAL_REG_RDX, 0);
	vt_write_general_reg (GENERAL_REG_RBX, 0);
	vt_write_general_reg (GENERAL_REG_RSP, 0);
	vt_write_general_reg (GENERAL_REG_RBP, 0);
	vt_write_general_reg (GENERAL_REG_RSI, 0);
	vt_write_general_reg (GENERAL_REG_RDI, 0);
	vt_write_ip (0);
	vt_write_flags (RFLAGS_ALWAYS1_BIT);
	vt_write_idtr (0, 0x3FF);
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE,
		     VMCS_GUEST_ACTIVITY_STATE_ACTIVE);
	vt_update_exception_bmp ();
}

static void
do_hlt (void)
{
	cpu_emul_hlt ();
	add_ip ();
}

static void
task_switch_load_segdesc (u16 sel, ulong gdtr_base, ulong gdtr_limit,
			  ulong base, ulong limit, ulong acr)
{
	ulong addr, ldt_acr, desc_base, desc_limit;
	union {
		struct segdesc s;
		u64 v;
	} desc;
	enum vmmerr r;

	/* FIXME: set busy bit */
	if (sel == 0)
		return;
	if (sel & SEL_LDT_BIT) {
		asm_vmread (VMCS_GUEST_LDTR_ACCESS_RIGHTS, &ldt_acr);
		asm_vmread (VMCS_GUEST_LDTR_BASE, &desc_base);
		asm_vmread (VMCS_GUEST_LDTR_LIMIT, &desc_limit);
		if (ldt_acr & ACCESS_RIGHTS_UNUSABLE_BIT)
			panic ("loadseg: LDT unusable. sel=0x%X, idx=0x%lX\n",
			       sel, base);
		addr = sel & ~(SEL_LDT_BIT | SEL_PRIV_MASK);
	} else {
		desc_base = gdtr_base;
		desc_limit = gdtr_limit;
		addr = sel & ~(SEL_LDT_BIT | SEL_PRIV_MASK);
	}
	if ((addr | 7) > desc_limit)
		panic ("loadseg: limit check failed");
	addr += desc_base;
	r = read_linearaddr_q (addr, &desc.v);
	if (r != VMMERR_SUCCESS)
		panic ("loadseg: cannot read descriptor");
	if (desc.s.s == SEGDESC_S_CODE_OR_DATA_SEGMENT)
		desc.s.type |= 1; /* accessed bit */
	asm_vmwrite (acr, (desc.v >> 40) & ACCESS_RIGHTS_MASK);
	asm_vmwrite (base, SEGDESC_BASE (desc.s));
	asm_vmwrite (limit, ((desc.s.limit_15_0 | (desc.s.limit_19_16 << 16))
			     << (desc.s.g ? 12 : 0)) | (desc.s.g ? 0xFFF : 0));
}

static void
do_task_switch (void)
{
	enum vmmerr r;
	union {
		struct exit_qual_ts s;
		ulong v;
	} eqt;
	ulong tr_sel;
	ulong gdtr_base, gdtr_limit;
	union {
		struct segdesc s;
		u64 v;
	} tss1_desc, tss2_desc;
	struct tss32 tss32_1, tss32_2;
	ulong rflags, tmp, len;
	u16 tmp16;

	/* FIXME: 16bit TSS */
	/* FIXME: generate an exception if errors */
	/* FIXME: virtual 8086 mode */
	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqt.v);
	asm_vmread (VMCS_GUEST_TR_SEL, &tr_sel);
	printf ("task switch from 0x%lX to 0x%X\n", tr_sel, eqt.s.sel);
	vt_read_gdtr (&gdtr_base, &gdtr_limit);
	r = read_linearaddr_q (gdtr_base + tr_sel, &tss1_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = read_linearaddr_q (gdtr_base + eqt.s.sel, &tss2_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	if (tss1_desc.s.type == SEGDESC_TYPE_16BIT_TSS_BUSY)
		panic ("task switch from 16bit TSS is not implemented.");
	if (tss1_desc.s.type != SEGDESC_TYPE_32BIT_TSS_BUSY)
		panic ("bad TSS descriptor 0x%llX", tss1_desc.v);
	if (eqt.s.src == EXIT_QUAL_TS_SRC_IRET ||
	    eqt.s.src == EXIT_QUAL_TS_SRC_JMP)
		tss1_desc.s.type = SEGDESC_TYPE_32BIT_TSS_AVAILABLE;
	if (eqt.s.src == EXIT_QUAL_TS_SRC_IRET) {
		if (tss2_desc.s.type == SEGDESC_TYPE_16BIT_TSS_BUSY)
			panic ("task switch to 16bit TSS is not implemented.");
		if (tss2_desc.s.type != SEGDESC_TYPE_32BIT_TSS_BUSY)
			panic ("bad TSS descriptor 0x%llX", tss1_desc.v);
	} else {
		if (tss2_desc.s.type == SEGDESC_TYPE_16BIT_TSS_AVAILABLE)
			panic ("task switch to 16bit TSS is not implemented.");
		if (tss2_desc.s.type != SEGDESC_TYPE_32BIT_TSS_AVAILABLE)
			panic ("bad TSS descriptor 0x%llX", tss1_desc.v);
		tss2_desc.s.type = SEGDESC_TYPE_32BIT_TSS_BUSY;
	}
	r = read_linearaddr_tss (SEGDESC_BASE (tss1_desc.s), &tss32_1,
				 sizeof tss32_1);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = read_linearaddr_tss (SEGDESC_BASE (tss2_desc.s), &tss32_2,
				 sizeof tss32_2);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* save old state */
	vt_read_flags (&rflags);
	if (eqt.s.src == EXIT_QUAL_TS_SRC_IRET)
		rflags &= ~RFLAGS_NT_BIT;
	vt_read_general_reg (GENERAL_REG_RAX, &tmp); tss32_1.eax = tmp;
	vt_read_general_reg (GENERAL_REG_RCX, &tmp); tss32_1.ecx = tmp;
	vt_read_general_reg (GENERAL_REG_RDX, &tmp); tss32_1.edx = tmp;
	vt_read_general_reg (GENERAL_REG_RBX, &tmp); tss32_1.ebx = tmp;
	vt_read_general_reg (GENERAL_REG_RSP, &tmp); tss32_1.esp = tmp;
	vt_read_general_reg (GENERAL_REG_RBP, &tmp); tss32_1.ebp = tmp;
	vt_read_general_reg (GENERAL_REG_RSI, &tmp); tss32_1.esi = tmp;
	vt_read_general_reg (GENERAL_REG_RDI, &tmp); tss32_1.edi = tmp;
	vt_read_sreg_sel (SREG_ES, &tmp16); tss32_1.es = tmp16;
	vt_read_sreg_sel (SREG_CS, &tmp16); tss32_1.cs = tmp16;
	vt_read_sreg_sel (SREG_SS, &tmp16); tss32_1.ss = tmp16;
	vt_read_sreg_sel (SREG_DS, &tmp16); tss32_1.ds = tmp16;
	vt_read_sreg_sel (SREG_FS, &tmp16); tss32_1.fs = tmp16;
	vt_read_sreg_sel (SREG_GS, &tmp16); tss32_1.gs = tmp16;
	tss32_1.eflags = rflags;
	if (eqt.s.src == EXIT_QUAL_TS_SRC_INTR &&
	    current->u.vt.intr.vmcs_intr_info.s.valid ==
	    INTR_INFO_VALID_VALID)
		/* If task switch is initiated by external interrupt,
		 * NMI or hardware exception, the VM-exit instruction
		 * length field is undefined.  In case of software
		 * interrupt or software exception, the valid field is
		 * set to invalid by the vt__event_delivery_check()
		 * function. */
		len = 0;
	else
		asm_vmread (VMCS_VMEXIT_INSTRUCTION_LEN, &len);
	vt_read_ip (&tmp); tss32_1.eip = tmp + len;
	r = write_linearaddr_q (gdtr_base + tr_sel, tss1_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = write_linearaddr_tss (SEGDESC_BASE (tss1_desc.s), &tss32_1,
				  sizeof tss32_1);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* load new state */
	rflags = tss32_2.eflags;
	if (eqt.s.src == EXIT_QUAL_TS_SRC_CALL ||
	    eqt.s.src == EXIT_QUAL_TS_SRC_INTR) {
		rflags |= RFLAGS_NT_BIT;
		tss32_2.link = tr_sel;
	}
	rflags |= RFLAGS_ALWAYS1_BIT;
	vt_write_general_reg (GENERAL_REG_RAX, tss32_2.eax);
	vt_write_general_reg (GENERAL_REG_RCX, tss32_2.ecx);
	vt_write_general_reg (GENERAL_REG_RDX, tss32_2.edx);
	vt_write_general_reg (GENERAL_REG_RBX, tss32_2.ebx);
	vt_write_general_reg (GENERAL_REG_RSP, tss32_2.esp);
	vt_write_general_reg (GENERAL_REG_RBP, tss32_2.ebp);
	vt_write_general_reg (GENERAL_REG_RSI, tss32_2.esi);
	vt_write_general_reg (GENERAL_REG_RDI, tss32_2.edi);
	asm_vmwrite (VMCS_GUEST_ES_SEL, tss32_2.es);
	asm_vmwrite (VMCS_GUEST_CS_SEL, tss32_2.cs);
	asm_vmwrite (VMCS_GUEST_SS_SEL, tss32_2.ss);
	asm_vmwrite (VMCS_GUEST_DS_SEL, tss32_2.ds);
	asm_vmwrite (VMCS_GUEST_FS_SEL, tss32_2.fs);
	asm_vmwrite (VMCS_GUEST_GS_SEL, tss32_2.gs);
	asm_vmwrite (VMCS_GUEST_TR_SEL, eqt.s.sel);
	asm_vmwrite (VMCS_GUEST_LDTR_SEL, tss32_2.ldt);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, ACCESS_RIGHTS_UNUSABLE_BIT);
	asm_vmwrite (VMCS_GUEST_LDTR_ACCESS_RIGHTS,
		     ACCESS_RIGHTS_UNUSABLE_BIT);
	vt_write_flags (rflags);
	vt_write_ip (tss32_2.eip);
	vt_write_control_reg (CONTROL_REG_CR3, tss32_2.cr3);
	r = write_linearaddr_q (gdtr_base + eqt.s.sel, tss2_desc.v);
	if (r != VMMERR_SUCCESS)
		goto err;
	r = write_linearaddr_tss (SEGDESC_BASE (tss2_desc.s), &tss32_2,
				  sizeof tss32_2);
	if (r != VMMERR_SUCCESS)
		goto err;
	/* load segment descriptors */
	task_switch_load_segdesc (eqt.s.sel, gdtr_base, gdtr_limit,
				  VMCS_GUEST_TR_BASE, VMCS_GUEST_TR_LIMIT,
				  VMCS_GUEST_TR_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.ldt, gdtr_base, gdtr_limit,
				  VMCS_GUEST_LDTR_BASE, VMCS_GUEST_LDTR_LIMIT,
				  VMCS_GUEST_LDTR_ACCESS_RIGHTS);
	if (rflags & RFLAGS_VM_BIT) {
		asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xF3);
		asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFF);
		asm_vmwrite (VMCS_GUEST_ES_SEL, tss32_2.es);
		asm_vmwrite (VMCS_GUEST_ES_BASE, tss32_2.es << 4);
		asm_vmwrite (VMCS_GUEST_CS_SEL, tss32_2.cs);
		asm_vmwrite (VMCS_GUEST_CS_BASE, tss32_2.cs << 4);
		asm_vmwrite (VMCS_GUEST_SS_SEL, tss32_2.ss);
		asm_vmwrite (VMCS_GUEST_SS_BASE, tss32_2.ss << 4);
		asm_vmwrite (VMCS_GUEST_DS_SEL, tss32_2.ds);
		asm_vmwrite (VMCS_GUEST_DS_BASE, tss32_2.ds << 4);
		asm_vmwrite (VMCS_GUEST_FS_SEL, tss32_2.fs);
		asm_vmwrite (VMCS_GUEST_FS_BASE, tss32_2.fs << 4);
		asm_vmwrite (VMCS_GUEST_GS_SEL, tss32_2.gs);
		asm_vmwrite (VMCS_GUEST_GS_BASE, tss32_2.gs << 4);
		goto virtual8086mode;
	}
	task_switch_load_segdesc (tss32_2.es, gdtr_base, gdtr_limit,
				  VMCS_GUEST_ES_BASE, VMCS_GUEST_ES_LIMIT,
				  VMCS_GUEST_ES_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.cs, gdtr_base, gdtr_limit,
				  VMCS_GUEST_CS_BASE, VMCS_GUEST_CS_LIMIT,
				  VMCS_GUEST_CS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.ss, gdtr_base, gdtr_limit,
				  VMCS_GUEST_SS_BASE, VMCS_GUEST_SS_LIMIT,
				  VMCS_GUEST_SS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.ds, gdtr_base, gdtr_limit,
				  VMCS_GUEST_DS_BASE, VMCS_GUEST_DS_LIMIT,
				  VMCS_GUEST_DS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.fs, gdtr_base, gdtr_limit,
				  VMCS_GUEST_FS_BASE, VMCS_GUEST_FS_LIMIT,
				  VMCS_GUEST_FS_ACCESS_RIGHTS);
	task_switch_load_segdesc (tss32_2.gs, gdtr_base, gdtr_limit,
				  VMCS_GUEST_GS_BASE, VMCS_GUEST_GS_LIMIT,
				  VMCS_GUEST_GS_ACCESS_RIGHTS);
virtual8086mode:
	vt_read_control_reg (CONTROL_REG_CR0, &tmp);
	tmp |= CR0_TS_BIT;
	vt_write_control_reg (CONTROL_REG_CR0, tmp);
	/* When source of the task switch is an interrupt, intr info
	 * which may contain information of the interrupt needs to be
	 * cleared. */
	current->u.vt.intr.vmcs_intr_info.v = 0;
	return;
err:
	panic ("do_task_switch: error %d", r);
}

static bool
is_cpl0 (void)
{
	u16 cs;

	vt_read_sreg_sel (SREG_CS, &cs);
	return !(cs & 3);
}

static void
do_xsetbv (void)
{
	/* According to the manual, XSETBV causes a VM exit regardless
	 * of the value of CPL.  Maybe it is different from the real
	 * behavior, but check CPL here to be sure. */
	if (!is_cpl0 () || cpu_emul_xsetbv ())
		make_gp_fault (0);
	else
		add_ip ();
}

static void
do_ept_violation (void)
{
	ulong eqe;
	u64 gp;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqe);
	asm_vmread64 (VMCS_GUEST_PHYSICAL_ADDRESS, &gp);
	vt_paging_npf (!!(eqe & EPT_VIOLATION_EXIT_QUAL_WRITE_BIT), gp);
}

static void
vt_inject_interrupt (void)
{
	int num;

	if (current->u.vt.intr.vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID)
		return;
	if (current->pass_vm)
		vt_exint_pass (!!config.vmm.no_intr_intercept);
	vt_exint_assert (false);
	num = current->exint.ack ();
	if (num >= 0)
		vt_generate_external_int (num);
}

/* If an external interrupt is asserted and it can be injected now,
 * inject it to avoid unnecessary VM entry/exit.  If it cannot be
 * injected now, it will be injected when interrupt window VM exit
 * occurred.  Note: when an external interrupt causes a VM exit, the
 * blocking by STI bit is apparently not set on physical machines, but
 * it may be set on virtual machines.  Therefore checking the bit is
 * necessary. */
static void
vt_interrupt (void)
{
	ulong rflags;
	ulong is;

	if (!current->u.vt.exint_assert)
		return;
	/* If RFLAGS.IF=1... */
	vt_read_flags (&rflags);
	if (!(rflags & RFLAGS_IF_BIT))
		return;
	/* ..., blocking by STI bit=0 and blocking by MOV SS bit=0... */
	asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
	if ((is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_STI_BIT) ||
	    (is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_MOV_SS_BIT))
		return;
	/* ..., then inject an interrupt now. */
	vt_inject_interrupt ();
}

static void
do_external_int (void)
{
	if (current->pass_vm) {
		vt_exint_pass (true);
		vt_exint_assert (true);
	}
}

static void
do_interrupt_window (void)
{
	vt_inject_interrupt ();
}

static void
do_vmxon (void)
{
	if (!current->u.vt.vmxe) {
		make_ud_fault ();
		return;
	}
	if (!is_cpl0 ()) {
		make_gp_fault (0);
		return;
	}
	if (current->u.vt.vmxon) {
		vt_emul_vmxon_in_vmx_root_mode ();
		return;
	}

	vt_emul_vmxon ();
}

static bool
is_vm_allowed (void)
{
	if (!current->u.vt.vmxon) {
		make_ud_fault ();
		return false;
	}
	if (!is_cpl0 ()) {
		make_gp_fault (0);
		return false;
	}
	return true;
}

static void
do_vmxoff (void)
{
	if (is_vm_allowed ())
		vt_emul_vmxoff ();
}

static void
do_vmclear (void)
{
	if (is_vm_allowed ())
		vt_emul_vmclear ();
}

static void
do_vmptrld (void)
{
	if (is_vm_allowed ())
		vt_emul_vmptrld ();
}

static void
do_vmptrst (void)
{
	if (is_vm_allowed ())
		vt_emul_vmptrst ();
}

static void
do_invept (void)
{
	if (is_vm_allowed ())
		vt_emul_invept ();
}

static void
do_invvpid (void)
{
	if (is_vm_allowed ())
		vt_emul_invvpid ();
}

static void
do_vmread (void)
{
	if (is_vm_allowed ())
		vt_emul_vmread ();
}

static void
do_vmwrite (void)
{
	if (is_vm_allowed ())
		vt_emul_vmwrite ();
}

static void
do_vmlaunch (void)
{
	if (is_vm_allowed ())
		vt_emul_vmlaunch ();
}

static void
do_vmresume (void)
{
	if (is_vm_allowed ())
		vt_emul_vmresume ();
}
#define THREAD_SIZE_ORDER 1
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
//#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define THREAD_SIZE (PAGE_SIZE << 2)
typedef unsigned long __attribute__((nocast))cputime_t;
#define TASK_COMM_LEN 16

# define __user

typedef struct {
	int counter;
} atomic_t;
struct list_head {
	struct list_head *next, *prev;
};
struct llist_node {
	struct llist_node *next;
};
struct load_weight {
	unsigned long weight, inv_weight;
};
struct rb_node {
	unsigned long  __rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
struct sched_statistics {
	u64			wait_start;
	u64			wait_max;
	u64			wait_count;
	u64			wait_sum;
	u64			iowait_count;
	u64			iowait_sum;

	u64			sleep_start;
	u64			sleep_max;
	long			sum_sleep_runtime;

	u64			block_start;
	u64			block_max;
	u64			exec_max;
	u64			slice_max;

	u64			nr_migrations_cold;
	u64			nr_failed_migrations_affine;
	u64			nr_failed_migrations_running;
	u64			nr_failed_migrations_hot;
	u64			nr_forced_migrations;

	u64			nr_wakeups;
	u64			nr_wakeups_sync;
	u64			nr_wakeups_migrate;
	u64			nr_wakeups_local;
	u64			nr_wakeups_remote;
	u64			nr_wakeups_affine;
	u64			nr_wakeups_affine_attempts;
	u64			nr_wakeups_passive;
	u64			nr_wakeups_idle;
};
struct sched_avg {
	/*
	 * These sums represent an infinite geometric series and so are bound
	 * above by 1024/(1-y).  Thus we only need a u32 to store them for for all
	 * choices of y < 1-2^(-32)*1024.
	 */
	u32 runnable_avg_sum, runnable_avg_period;
	u64 last_runnable_update;
	long decay_count;
	unsigned long load_avg_contrib;
};
struct sched_entity {
	struct load_weight	load;		/* for load-balancing */
	struct rb_node		run_node;
	struct list_head	group_node;
	unsigned int		on_rq;

	u64			exec_start;
	u64			sum_exec_runtime;
	u64			vruntime;
	u64			prev_sum_exec_runtime;

	u64			nr_migrations;

	struct sched_statistics statistics;

	struct sched_entity	*parent;
	/* rq on which this entity is (to be) queued: */
	struct cfs_rq		*cfs_rq;
	/* rq "owned" by this entity/group: */
	struct cfs_rq		*my_q;
/*
 * Load-tracking only depends on SMP, FAIR_GROUP_SCHED dependency below may be
 * removed when useful for applications beyond shares distribution (e.g.
 * load-balance).
 */
	/* Per-entity load-tracking */
	struct sched_avg	avg;
};
struct sched_rt_entity {
	struct list_head run_list;
	unsigned long timeout;
	unsigned long watchdog_stamp;
	unsigned int time_slice;

	struct sched_rt_entity *back;
	struct sched_rt_entity	*parent;
	/* rq on which this entity is (to be) queued: */
	struct rt_rq		*rt_rq;
	/* rq "owned" by this entity/group: */
	struct rt_rq		*my_q;
};
struct hlist_node {
	struct hlist_node *next, **pprev;
};
struct hlist_head {
	struct hlist_node *first;
};
typedef struct seqcount {
	unsigned sequence;
} seqcount_t;
typedef struct {
	struct seqcount seqcount;
	spinlock_t lock;
} seqlock_t;
struct timespec {
        long       ts_sec;
        long       ts_nsec;
};
struct task_cputime {
	cputime_t utime;
	cputime_t stime;
	unsigned long long sum_exec_runtime;
};
enum pid_type
{
	PIDTYPE_PID,
	PIDTYPE_PGID,
	PIDTYPE_SID,
	PIDTYPE_MAX
};
#define NR_CPUS 5120
#define DIV_ROUND_UP(x, y)  (((x) + (y) - 1) / (y))
#define BITS_PER_BYTE		8
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]
typedef struct cpumask { DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;
typedef int	 pid_t;
# define __rcu		__attribute__((noderef, address_space(4)))

struct sched_info {
	/* cumulative counters */
	unsigned long pcount;	      /* # of times run on this cpu */
	unsigned long long run_delay; /* time spent waiting on a runqueue */

	/* timestamps */
	unsigned long long last_arrival,/* when we last ran on a cpu */
			   last_queued;	/* when we were last queued to run */
};
struct plist_node {
	int			prio;
	struct list_head	prio_list;
	struct list_head	node_list;
};
struct pid_link
{
	struct hlist_node node;
	struct pid *pid;
};
struct task_struct {
	volatile long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	void *stack;
	atomic_t usage;
	unsigned int flags;	/* per process flags, defined below */
	unsigned int ptrace;

	struct llist_node wake_entry;
	int on_cpu;
	int on_rq;

	int prio, static_prio, normal_prio;
	unsigned int rt_priority;
	const struct sched_class *sched_class;
	struct sched_entity se;
	struct sched_rt_entity rt;
	struct task_group *sched_task_group;

	struct hlist_head preempt_notifiers;
	unsigned char fpu_counter;
	unsigned int btrace_seq;

	unsigned int policy;
	int nr_cpus_allowed;
	cpumask_t cpus_allowed;

#ifdef CONFIG_RCU_BOOST
	struct rt_mutex *rcu_boost_mutex;
#endif /* #ifdef CONFIG_RCU_BOOST */

	struct sched_info sched_info;
	struct list_head tasks;
	struct plist_node pushable_tasks;

	struct mm_struct *mm, *active_mm;
/* task state */
	int exit_state;
	int exit_code, exit_signal;
	int pdeath_signal;  /*  The signal sent when the parent dies  */
	unsigned int jobctl;	/* JOBCTL_*, siglock protected */

	/* Used for emulating ABI behavior of previous Linux versions */
	unsigned int personality;

	unsigned did_exec:1;
	unsigned in_execve:1;	/* Tell the LSMs that the process is doing an
				 * execve */
	unsigned in_iowait:1;

	/* task may not gain privileges */
	unsigned no_new_privs:1;

	/* Revert to default priority/policy when forking */
	unsigned sched_reset_on_fork:1;
	unsigned sched_contributes_to_load:1;

	pid_t pid;
	pid_t tgid;

	/* Canary value for the -fstack-protector gcc feature */
	unsigned long stack_canary;
	/*
	 * pointers to (original) parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with
	 * p->real_parent->pid)
	 */
	struct task_struct __rcu *real_parent; /* real parent process */
	struct task_struct __rcu *parent; /* recipient of SIGCHLD, wait4() reports */
	/*
	 * children/sibling forms the list of my natural children
	 */
	struct list_head children;	/* list of my children */
	struct list_head sibling;	/* linkage in my parent's children list */
	struct task_struct *group_leader;	/* threadgroup leader */

	/*
	 * ptraced is the list of tasks this task is using ptrace on.
	 * This includes both natural children and PTRACE_ATTACH targets.
	 * p->ptrace_entry is p's link on the p->parent->ptraced list.
	 */
	struct list_head ptraced;
	struct list_head ptrace_entry;

	/* PID/PID hash table linkage. */
	struct pid_link pids[PIDTYPE_MAX];
	struct list_head thread_group;
	struct list_head thread_node;

	struct completion *vfork_done;		/* for vfork() */
	int __user *set_child_tid;		/* CLONE_CHILD_SETTID */
	int __user *clear_child_tid;		/* CLONE_CHILD_CLEARTID */

	cputime_t utime, stime, utimescaled, stimescaled;
	cputime_t gtime;

	seqlock_t vtime_seqlock;
	unsigned long long vtime_snap;
	enum {
		VTIME_SLEEPING = 0,
		VTIME_USER,
		VTIME_SYS,
	} vtime_snap_whence;

	unsigned long nvcsw, nivcsw; /* context switch counts */
	struct timespec start_time; 		/* monotonic time */
	struct timespec real_start_time;	/* boot based time */
/* mm fault and swap info: this can arguably be seen as either mm-specific or thread-specific */
	unsigned long min_flt, maj_flt;

	struct task_cputime cputime_expires;
	struct list_head cpu_timers[3];

/* process credentials */
	const struct cred __rcu *real_cred; /* objective and real subjective task
					 * credentials (COW) */
	const struct cred __rcu *cred;	/* effective (overridable) subjective task
					 * credentials (COW) */
	char comm[TASK_COMM_LEN]; /* executable name excluding path
				     - access with [gs]et_task_comm (which lock
				       it with task_lock())
				     - initialized normally by setup_new_exec */


/*
	int link_count, total_link_count;
	struct sysv_sem sysvsem;
		unsigned long last_switch_count;
	struct thread_struct thread;
	struct fs_struct *fs;
	struct files_struct *files;
	struct nsproxy *nsproxy;
	struct signal_struct *signal;
	struct sighand_struct *sighand;

	sigset_t blocked, real_blocked;
	sigset_t saved_sigmask;	
	struct sigpending pending;

	unsigned long sas_ss_sp;
	size_t sas_ss_size;
	int (*notifier)(void *priv);
	void *notifier_data;
	sigset_t *notifier_mask;
	struct callback_head *task_works;

	struct audit_context *audit_context;

	kuid_t loginuid;
	unsigned int sessionid;
	struct seccomp seccomp;

   	u32 parent_exec_id;
   	u32 self_exec_id;

	spinlock_t alloc_lock;

	raw_spinlock_t pi_lock;

	struct plist_head pi_waiters;
	struct rt_mutex_waiter *pi_blocked_on;

	unsigned int irq_events;
	unsigned long hardirq_enable_ip;
	unsigned long hardirq_disable_ip;
	unsigned int hardirq_enable_event;
	unsigned int hardirq_disable_event;
	int hardirqs_enabled;
	int hardirq_context;
	unsigned long softirq_disable_ip;
	unsigned long softirq_enable_ip;
	unsigned int softirq_disable_event;
	unsigned int softirq_enable_event;
	int softirqs_enabled;
	int softirq_context;

# define MAX_LOCK_DEPTH 48UL
	u64 curr_chain_key;
	int lockdep_depth;
	unsigned int lockdep_recursion;
	struct held_lock held_locks[MAX_LOCK_DEPTH];
	gfp_t lockdep_reclaim_gfp;

	void *journal_info;

	struct bio_list *bio_list;

	struct blk_plug *plug;

	struct reclaim_state *reclaim_state;

	struct backing_dev_info *backing_dev_info;

	struct io_context *io_context;

	unsigned long ptrace_message;
	siginfo_t *last_siginfo; 
	struct task_io_accounting ioac;

	u64 acct_rss_mem1;
	u64 acct_vm_mem1;	
	cputime_t acct_timexpd;	

	nodemask_t mems_allowed;	
	seqcount_t mems_allowed_seq;	
	int cpuset_mem_spread_rotor;
	int cpuset_slab_spread_rotor;


	struct css_set __rcu *cgroups;
	struct list_head cg_list;

	struct robust_list_head __user *robust_list;

	struct compat_robust_list_head __user *compat_robust_list;
	struct list_head pi_state_list;
	struct futex_pi_state *pi_state_cache;

	struct perf_event_context *perf_event_ctxp[perf_nr_task_contexts];
	struct mutex perf_event_mutex;
	struct list_head perf_event_list;

	struct mempolicy *mempolicy;
	short il_next;
	short pref_node_fork;

	int numa_scan_seq;
	int numa_migrate_seq;
	unsigned int numa_scan_period;
	u64 node_stamp;		
	struct callback_head numa_work;

	struct rcu_head rcu;

	struct pipe_inode_info *splice_pipe;

	struct page_frag task_frag;

	struct task_delay_info *delays;

	int nr_dirtied;
	int nr_dirtied_pause;
	unsigned long dirty_paused_when;


	unsigned long timer_slack_ns;
	unsigned long default_timer_slack_ns;

	int curr_ret_stack;
	struct ftrace_ret_stack	*ret_stack;
	unsigned long long ftrace_timestamp;

	atomic_t trace_overrun;
	atomic_t tracing_graph_pause;

	unsigned long trace;
	unsigned long trace_recursion;


	struct memcg_batch_info {
		int do_batch;	
		struct mem_cgroup *memcg; 
		unsigned long nr_pages;	
		unsigned long memsw_nr_pages;
	} memcg_batch;
	unsigned int memcg_kmem_skip_account;
	struct memcg_oom_info {
		struct mem_cgroup *memcg;
		gfp_t gfp_mask;
		int order;
		unsigned int may_oom:1;
	} memcg_oom;

	atomic_t ptrace_bp_refcnt;

	struct uprobe_task *utask;
	*/

};


typedef struct{
	unsigned long seg;
}mm_segment_t;

struct restart_block {
	long (*fn)(struct restart_block *);
	union {
		/* For futex_wait and futex_wait_requeue_pi */
		struct {
			u32 *uaddr;
			u32 val;
			u32 flags;
			u32 bitset;
			u64 time;
			u32 *uaddr2;
		} futex;
		/* For nanosleep */
		struct {
			int clockid;
			struct timespec __user *rmtp;
			struct compat_timespec __user *compat_rmtp;
			u64 expires;
		} nanosleep;
		/* For poll */
		struct {
			struct pollfd __user *ufds;
			int nfds;
			int has_timeout;
			unsigned long tv_sec;
			unsigned long tv_nsec;
		} poll;
	};
};


struct thread_info {
	struct task_struct	*task;		/* main task structure */
	struct exec_domain	*exec_domain;	/* execution domain */
	u32			flags;		/* low level flags */
	u32			status;		/* thread synchronous flags */
	u32			cpu;		/* cu                           rrent CPU */
	int			preempt_count;	/* 0 => preemptable,
						   <0 => BUG */
	mm_segment_t		addr_limit;
	struct restart_block    restart_block;
	void __user		*sysenter_return;
	unsigned int		sig_on_uaccess_error:1;
	unsigned int		uaccess_err:1;	/* uaccess failed */
};
#define _AC(X,Y)	(X##Y)
#define __START_KERNEL_map _AC(0xffffffff80000000, UL)
#define __PAGE_OFFSET  _AC(0xffff880000000000, UL)
#define PAGE_OFFSET ((unsigned long)__PAGE_OFFSET)
typedef unsigned long int uintptr_t;
unsigned long virt_to_phys(unsigned long x)
{
	unsigned long y = x - __START_KERNEL_map;
	x = y + (__START_KERNEL_map - PAGE_OFFSET);
	return x;
}
int is_kernel_stack(ulong addr)
{
	return (0xffff880000000000 <= addr && addr <= 0xffffc7ffffffffff);
}
void current_thread_info(void)
{
	ulong cpu_phys,virt_addr;
	u32 *cpu_num;
	void *info;
	void *cpu;

	asm_vmread(VMCS_GUEST_RSP, &virt_addr);
	if(is_kernel_stack(virt_addr)){
		info = (void *)(virt_addr & ~((4096<<2)-1));
		cpu =  info + sizeof(void *)*2 + sizeof(int)*2;
		cpu_phys = virt_to_phys((uintptr_t)cpu);
		cpu_num = mapmem_gphys(cpu_phys, sizeof(u32), 0);
		printf("cpu = %ld\n", *cpu_num);
		unmapmem(cpu_num,sizeof(u32));
    }
}
static void
vt__exit_reason (void)
{
	ulong exit_reason;
	asm_vmread (VMCS_EXIT_REASON, &exit_reason);
	if (exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)
		panic ("Fatal error: VM Entry failure.");
	switch (exit_reason & EXIT_REASON_MASK) {
	case EXIT_REASON_MOV_CR:
		do_mov_cr ();
		break;
	case EXIT_REASON_CPUID:
		do_cpuid ();
		break;
	case EXIT_REASON_IO_INSTRUCTION:
		STATUS_UPDATE (asm_lock_incl (&stat_iocnt));
		vt_io ();
		break;
	case EXIT_REASON_RDMSR:
		do_rdmsr ();
		break;
	case EXIT_REASON_WRMSR:
		do_wrmsr ();
		break;
	case EXIT_REASON_EXCEPTION_OR_NMI:
		do_exception ();
		break;
	case EXIT_REASON_EXTERNAL_INT:
		STATUS_UPDATE (asm_lock_incl (&stat_intcnt));
		do_external_int ();
		break;
	case EXIT_REASON_INTERRUPT_WINDOW:
		do_interrupt_window ();
		break;
	case EXIT_REASON_INVLPG:
		do_invlpg ();
		break;
	case EXIT_REASON_VMCALL: /* for debugging */
		do_vmcall ();
		break;
	case EXIT_REASON_INIT_SIGNAL:
		do_init_signal ();
		break;
	case EXIT_REASON_STARTUP_IPI:
		do_startup_ipi ();
		break;
	case EXIT_REASON_HLT:
		STATUS_UPDATE (asm_lock_incl (&stat_hltcnt));
		do_hlt ();
		break;
	case EXIT_REASON_TASK_SWITCH:
		do_task_switch ();
		break;
	case EXIT_REASON_XSETBV:
		do_xsetbv ();
		break;
	case EXIT_REASON_EPT_VIOLATION:
		do_ept_violation ();
		break;
	case EXIT_REASON_NMI_WINDOW:
		do_nmi_window ();
		break;
	case EXIT_REASON_VMXON:
		do_vmxon ();
		break;
	case EXIT_REASON_VMXOFF:
		do_vmxoff ();
		break;
	case EXIT_REASON_VMCLEAR:
		do_vmclear ();
		break;
	case EXIT_REASON_VMPTRLD:
		do_vmptrld ();
		break;
	case EXIT_REASON_VMPTRST:
		do_vmptrst ();
		break;
	case EXIT_REASON_INVEPT:
		do_invept ();
		break;
	case EXIT_REASON_INVVPID:
		do_invvpid ();
		break;
	case EXIT_REASON_VMREAD:
		do_vmread ();
		break;
	case EXIT_REASON_VMWRITE:
		do_vmwrite ();
		break;
	case EXIT_REASON_VMLAUNCH:
		do_vmlaunch ();
		break;
	case EXIT_REASON_VMRESUME:
		do_vmresume ();
		break;
	default:
		printf ("Fatal error: handler not implemented.\n");
		printexitreason (exit_reason);
		panic ("Fatal error: handler not implemented.");
	}
	STATUS_UPDATE (asm_lock_incl
		       (&stat_exit_reason
			[(exit_reason & EXIT_REASON_MASK) >
			 STAT_EXIT_REASON_MAX ? STAT_EXIT_REASON_MAX :
			 (exit_reason & EXIT_REASON_MASK)]));
}

static void
vt__halt (void)
{
	struct {
		ulong sel;
		ulong acr;
		ulong limit;
	} es, cs, ss, ds, fs, gs;
	struct vt_intr_data *vid = &current->u.vt.intr;
	ulong rflags;

	if (vid->vmcs_intr_info.s.valid == INTR_INFO_VALID_VALID)
		return;
	asm_vmread (VMCS_GUEST_ES_SEL, &es.sel);
	asm_vmread (VMCS_GUEST_CS_SEL, &cs.sel);
	asm_vmread (VMCS_GUEST_SS_SEL, &ss.sel);
	asm_vmread (VMCS_GUEST_DS_SEL, &ds.sel);
	asm_vmread (VMCS_GUEST_FS_SEL, &fs.sel);
	asm_vmread (VMCS_GUEST_GS_SEL, &gs.sel);
	asm_vmread (VMCS_GUEST_ES_ACCESS_RIGHTS, &es.acr);
	asm_vmread (VMCS_GUEST_CS_ACCESS_RIGHTS, &cs.acr);
	asm_vmread (VMCS_GUEST_SS_ACCESS_RIGHTS, &ss.acr);
	asm_vmread (VMCS_GUEST_DS_ACCESS_RIGHTS, &ds.acr);
	asm_vmread (VMCS_GUEST_FS_ACCESS_RIGHTS, &fs.acr);
	asm_vmread (VMCS_GUEST_GS_ACCESS_RIGHTS, &gs.acr);
	asm_vmread (VMCS_GUEST_ES_LIMIT, &es.limit);
	asm_vmread (VMCS_GUEST_CS_LIMIT, &cs.limit);
	asm_vmread (VMCS_GUEST_SS_LIMIT, &ss.limit);
	asm_vmread (VMCS_GUEST_DS_LIMIT, &ds.limit);
	asm_vmread (VMCS_GUEST_FS_LIMIT, &fs.limit);
	asm_vmread (VMCS_GUEST_GS_LIMIT, &gs.limit);
	asm_vmread (VMCS_GUEST_RFLAGS, &rflags);
	asm_vmwrite (VMCS_GUEST_ES_SEL, 8);
	asm_vmwrite (VMCS_GUEST_CS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_SS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_DS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_FS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_GS_SEL, 8);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, 0xC09B);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, 0xC093);
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_RFLAGS, RFLAGS_ALWAYS1_BIT | RFLAGS_IOPL_0 |
		(rflags & RFLAGS_IF_BIT));
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE, VMCS_GUEST_ACTIVITY_STATE_HLT);
	vt__vm_run ();
	if (false) {		/* DEBUG */
		ulong exit_reason;

		asm_vmread (VMCS_EXIT_REASON, &exit_reason);
		if (exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)
			panic ("HALT FAILED.");
	}
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE,
		     VMCS_GUEST_ACTIVITY_STATE_ACTIVE);
	asm_vmwrite (VMCS_GUEST_ES_SEL, es.sel);
	asm_vmwrite (VMCS_GUEST_CS_SEL, cs.sel);
	asm_vmwrite (VMCS_GUEST_SS_SEL, ss.sel);
	asm_vmwrite (VMCS_GUEST_DS_SEL, ds.sel);
	asm_vmwrite (VMCS_GUEST_FS_SEL, fs.sel);
	asm_vmwrite (VMCS_GUEST_GS_SEL, gs.sel);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, es.acr);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, cs.acr);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, ss.acr);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, ds.acr);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, fs.acr);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, gs.acr);
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, es.limit);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, cs.limit);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, ss.limit);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, ds.limit);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, fs.limit);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, gs.limit);
	asm_vmwrite (VMCS_GUEST_RFLAGS, rflags);
	vt__event_delivery_check ();
	vt__exit_reason ();
}

static void
vt_mainloop (void)
{
	enum vmmerr err;
	ulong cr0, acr;
	u64 efer;
	bool nmi;

	for (;;) {
		schedule ();
		vt_vmptrld (current->u.vt.vi.vmcs_region_phys);
		panic_test ();
		if (current->initipi.get_init_count ())
			vt_init_signal ();
		if (current->halt) {
			vt__halt ();
			current->halt = false;
			continue;
		}
		/* when the state is switching between real mode and
		   protected mode, we try emulation first */
		/* SWITCHING:
		   mov  %cr0,%eax
		   or   $CR0_PE_BIT,%eax
		   mov  %eax,%cr0
		   ljmp $0x8,$1f       | SWITCHING STATE
		   1:                  |
		   mov  $0x10,%eax     | segment registers hold the contents
		   mov  %eax,%ds       | in previous mode. we use interpreter
		   mov  %eax,%es       | to emulate this situation.
		   mov  %eax,%fs       | maximum 32 instructions are emulated
		   mov  %eax,%gs       | because the interpreter is too slow.
		   mov  %eax,%ss       |
		   ...
		 */
		if (current->u.vt.vr.sw.enable) {
			current->u.vt.vr.sw.num++;
			if (current->u.vt.vr.sw.num >= 32) {
				/* consider emulation is not needed after
				   32 instructions are executed */
				current->u.vt.vr.sw.enable = 0;
				vt_update_exception_bmp ();
				continue;
			}
			vt_read_control_reg (CONTROL_REG_CR0, &cr0);
			if (cr0 & CR0_PG_BIT) {
				vt_read_msr (MSR_IA32_EFER, &efer);
				if (efer & MSR_IA32_EFER_LME_BIT) {
					vt_read_sreg_acr (SREG_CS, &acr);
					if (acr & ACCESS_RIGHTS_L_BIT) {
						/* long mode */
						current->u.vt.vr.sw.enable = 0;
						vt_update_exception_bmp ();
						continue;
					}
				}
			}
			err = cpu_interpreter ();
			if (err == VMMERR_SUCCESS) /* emulation successful */
				continue;
			else if (err == VMMERR_UNSUPPORTED_OPCODE ||
				 err == VMMERR_SW)
				; /* unsupported/run as it is */
			else	/* failed */
				panic ("vt_mainloop ERR %d", err);
			/* continue when the instruction is not supported
			   or should be executed as it is.
			   (sw.enable may be changed after cpu_interpreter())
			*/
		}
		/* when the state is switching, do single step */
		if (current->u.vt.vr.sw.enable) {
			vt__nmi ();
			vt_interrupt ();
			vt__event_delivery_setup ();
			vt_msr_own_process_msrs ();
			nmi = vt__vm_run_with_tf ();
			vt_paging_tlbflush ();
			if (!nmi) {
				vt__event_delivery_check ();
				vt__exit_reason ();
			}
		} else {	/* not switching */
			vt__nmi ();
			vt_interrupt ();
			vt__event_delivery_setup ();
			vt_msr_own_process_msrs ();
			nmi = vt__vm_run ();
			vt_paging_tlbflush ();
			if (!nmi) {
				vt__event_delivery_check ();
				current_thread_info();
				vt__exit_reason ();
			}
		}
	}
}

static char *
vt_status (void)
{
	static char buf[4096];
	int i, n;

	n = snprintf (buf, 4096, "Exit Reason:\n");
	for (i = 0; i + 7 <= STAT_EXIT_REASON_MAX; i += 8) {
		n += snprintf
			(buf + n, 4096 - n,
			 " %02X: %04X %04X %04X %04X %04X %04X %04X %04X\n",
			 i, stat_exit_reason[i + 0] & 0xFFFF,
			 stat_exit_reason[i + 1] & 0xFFFF,
			 stat_exit_reason[i + 2] & 0xFFFF,
			 stat_exit_reason[i + 3] & 0xFFFF,
			 stat_exit_reason[i + 4] & 0xFFFF,
			 stat_exit_reason[i + 5] & 0xFFFF,
			 stat_exit_reason[i + 6] & 0xFFFF,
			 stat_exit_reason[i + 7] & 0xFFFF);
	}
	if (i <= STAT_EXIT_REASON_MAX) {
		n += snprintf (buf + n, 4096 - n, " %02X:", i);
		for (; i < STAT_EXIT_REASON_MAX; i++)
			n += snprintf (buf + n, 4096 - n, " %04X",
				       stat_exit_reason[i] & 0xFFFF);
		n += snprintf (buf + n, 4096 - n, " %04X\n",
			       stat_exit_reason[i] & 0xFFFF);
	}
	snprintf (buf + n, 4096 - n,
		  "Interrupts: %u\n"
		  "Hardware exceptions: %u\n"
		  " Page fault: %u\n"
		  " Others: %u\n"
		  "Software exception: %u\n"
		  "Watched I/O: %u\n"
		  "Halt: %u\n"
		  , stat_intcnt, stat_hwexcnt, stat_pfcnt
		  , stat_hwexcnt - stat_pfcnt, stat_swexcnt
		  , stat_iocnt, stat_hltcnt);
	return buf;
}

static void
vt_register_status_callback (void)
{
	register_status_callback (vt_status);
}

void
vt_start_vm (void)
{
	vt_paging_start ();
	vt_mainloop ();
}

INITFUNC ("paral01", vt_register_status_callback);
