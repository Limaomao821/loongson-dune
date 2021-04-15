#ifndef INTERNAL_H_6IUWCEFP
#define INTERNAL_H_6IUWCEFP

/* Loongarch KVM register ids */
#define LOONGARCH_CSR_32(_R, _S)					\
	(KVM_REG_LOONGARCH_CSR | KVM_REG_SIZE_U32 | (8 * (_R) + (_S)))

#define LOONGARCH_CSR_64(_R, _S)					\
	(KVM_REG_LOONGARCH_CSR | KVM_REG_SIZE_U64 | (8 * (_R) + (_S)))

#define KVM_LOONGARCH_CSR_CRMD		0
#define KVM_LOONGARCH_CSR_PRMD		1
#define KVM_LOONGARCH_CSR_EUEN		2
#define KVM_LOONGARCH_CSR_MISC		3
#define KVM_LOONGARCH_CSR_ECFG		4
#define KVM_LOONGARCH_CSR_ESTAT		5
#define KVM_LOONGARCH_CSR_EPC		  6
#define KVM_LOONGARCH_CSR_BADV		7
#define KVM_LOONGARCH_CSR_BADI		8
#define KVM_LOONGARCH_CSR_EBASE		0xc
#define KVM_LOONGARCH_CSR_TLBIDX	0x10
#define KVM_LOONGARCH_CSR_TLBHI		0x11
#define KVM_LOONGARCH_CSR_TLBLO0	0x12
#define KVM_LOONGARCH_CSR_TLBLO1	0x13
#define KVM_LOONGARCH_CSR_GTLBC		0x15
#define KVM_LOONGARCH_CSR_TRGP		0x16
#define KVM_LOONGARCH_CSR_ASID		0x18
#define KVM_LOONGARCH_CSR_PGDL		0x19
#define KVM_LOONGARCH_CSR_PGDH		0x1a
#define KVM_LOONGARCH_CSR_PGD		  0x1b
#define KVM_LOONGARCH_CSR_PWCTL0	0x1c
#define KVM_LOONGARCH_CSR_PWCTL1	0x1d
#define KVM_LOONGARCH_CSR_STLBPS	0x1e
#define KVM_LOONGARCH_CSR_RVACFG	0x1f
#define KVM_LOONGARCH_CSR_CPUNUM	0x20
#define KVM_LOONGARCH_CSR_PRCFG1	0x21
#define KVM_LOONGARCH_CSR_PRCFG2	0x22
#define KVM_LOONGARCH_CSR_PRCFG3	0x23
#define KVM_LOONGARCH_CSR_KSCRATCH0	0x30
#define KVM_LOONGARCH_CSR_KSCRATCH1	0x31
#define KVM_LOONGARCH_CSR_KSCRATCH2	0x32
#define KVM_LOONGARCH_CSR_KSCRATCH3	0x33
#define KVM_LOONGARCH_CSR_KSCRATCH4	0x34
#define KVM_LOONGARCH_CSR_KSCRATCH5	0x35
#define KVM_LOONGARCH_CSR_KSCRATCH6	0x36
#define KVM_LOONGARCH_CSR_KSCRATCH7	0x37
#define KVM_LOONGARCH_CSR_KSCRATCH8	0x38
#define KVM_LOONGARCH_CSR_TIMERID	  0x40
#define KVM_LOONGARCH_CSR_TIMERCFG	0x41
#define KVM_LOONGARCH_CSR_TIMERTICK	0x42
#define KVM_LOONGARCH_CSR_TIMEROFFSET	0x43
#define KVM_LOONGARCH_CSR_GSTAT		0x50
#define KVM_LOONGARCH_CSR_GCFG		0x51
#define KVM_LOONGARCH_CSR_GINTC		0x52
#define KVM_LOONGARCH_CSR_GCNTC		0x53
#define KVM_LOONGARCH_CSR_LLBCTL	0x60
#define KVM_LOONGARCH_CSR_IMPCTL1	0x80
#define KVM_LOONGARCH_CSR_IMPCTL2	0x81
#define KVM_LOONGARCH_CSR_GNMI		0x82
#define KVM_LOONGARCH_CSR_TLBREBASE	0x88
#define KVM_LOONGARCH_CSR_TLBRBADV	0x89
#define KVM_LOONGARCH_CSR_TLBREPC	0x8a
#define KVM_LOONGARCH_CSR_TLBRSAVE	0x8b
#define KVM_LOONGARCH_CSR_TLBRELO0	0x8c
#define KVM_LOONGARCH_CSR_TLBRELO1	0x8d
#define KVM_LOONGARCH_CSR_TLBREHI	0x8e
#define KVM_LOONGARCH_CSR_TLBRPRMD	0x8f
#define KVM_LOONGARCH_CSR_ERRCTL	0x90
#define KVM_LOONGARCH_CSR_ERRINFO1	0x91
#define KVM_LOONGARCH_CSR_ERRINFO2	0x92
#define KVM_LOONGARCH_CSR_ERREBASE	0x93
#define KVM_LOONGARCH_CSR_ERREPC	0x94
#define KVM_LOONGARCH_CSR_ERRSAVE	0x95
#define KVM_LOONGARCH_CSR_CTAG		0x98
#define KVM_LOONGARCH_CSR_MCSR0		0xc0
#define KVM_LOONGARCH_CSR_MCSR1		0xc1
#define KVM_LOONGARCH_CSR_MCSR2		0xc2
#define KVM_LOONGARCH_CSR_MCSR3		0xc3
#define KVM_LOONGARCH_CSR_MCSR8		0xc8
#define KVM_LOONGARCH_CSR_MCSR9		0xc9
#define KVM_LOONGARCH_CSR_MCSR10	0xca
#define KVM_LOONGARCH_CSR_MCSR24	0xf0
#define KVM_LOONGARCH_CSR_UCAWIN	0x100
#define KVM_LOONGARCH_CSR_UCAWIN0_LO	0x102
#define KVM_LOONGARCH_CSR_UCAWIN0_HI	0x103
#define KVM_LOONGARCH_CSR_UCAWIN1_LO	0x104
#define KVM_LOONGARCH_CSR_UCAWIN1_HI	0x105
#define KVM_LOONGARCH_CSR_UCAWIN2_LO	0x106
#define KVM_LOONGARCH_CSR_UCAWIN2_HI	0x107
#define KVM_LOONGARCH_CSR_UCAWIN3_LO	0x108
#define KVM_LOONGARCH_CSR_UCAWIN3_HI	0x109
#define KVM_LOONGARCH_CSR_DMWIN0	0x180
#define KVM_LOONGARCH_CSR_DMWIN1	0x181
#define KVM_LOONGARCH_CSR_DMWIN2	0x182
#define KVM_LOONGARCH_CSR_DMWIN3	0x183
#define KVM_LOONGARCH_CSR_PERF0_EVENT	0x200
#define KVM_LOONGARCH_CSR_PERF0_COUNT	0x201
#define KVM_LOONGARCH_CSR_PERF1_EVENT	0x202
#define KVM_LOONGARCH_CSR_PERF1_COUNT	0x203
#define KVM_LOONGARCH_CSR_PERF2_EVENT	0x204
#define KVM_LOONGARCH_CSR_PERF2_COUNT	0x205
#define KVM_LOONGARCH_CSR_PERF3_EVENT	0x206
#define KVM_LOONGARCH_CSR_PERF3_COUNT	0x207
#define KVM_LOONGARCH_CSR_DEBUG		0x500
#define KVM_LOONGARCH_CSR_DEPC		0x501
#define KVM_LOONGARCH_CSR_DESAVE	0x502

#define KVM_CSR_CRMD		LOONGARCH_CSR_64(0, 0)
#define KVM_CSR_PRMD		LOONGARCH_CSR_64(1, 0)
#define KVM_CSR_EUEN		LOONGARCH_CSR_64(2, 0)
#define KVM_CSR_MISC		LOONGARCH_CSR_64(3, 0)
#define KVM_CSR_ECFG		LOONGARCH_CSR_64(4, 0)
#define KVM_CSR_ESTAT		LOONGARCH_CSR_64(5, 0)
#define KVM_CSR_EPC		LOONGARCH_CSR_64(6, 0)
#define KVM_CSR_BADV		LOONGARCH_CSR_64(7, 0)
#define KVM_CSR_BADI		LOONGARCH_CSR_64(8, 0)
#define KVM_CSR_EBASE		LOONGARCH_CSR_64(0xc, 0)
#define KVM_CSR_TLBIDX		LOONGARCH_CSR_64(0x10, 0)
#define KVM_CSR_TLBHI		LOONGARCH_CSR_64(0x11, 0)
#define KVM_CSR_TLBLO0		LOONGARCH_CSR_64(0x12, 0)
#define KVM_CSR_TLBLO1		LOONGARCH_CSR_64(0x13, 0)
#define KVM_CSR_GTLBC		LOONGARCH_CSR_64(0x15, 0)
#define KVM_CSR_TRGP		LOONGARCH_CSR_64(0x16, 0)
#define KVM_CSR_ASID		LOONGARCH_CSR_64(0x18, 0)
#define KVM_CSR_PGDL		LOONGARCH_CSR_64(0x19, 0)
#define KVM_CSR_PGDH		LOONGARCH_CSR_64(0x1a, 0)
#define KVM_CSR_PGD		LOONGARCH_CSR_64(0x1b, 0)
#define KVM_CSR_PWCTL0		LOONGARCH_CSR_64(0x1c, 0)
#define KVM_CSR_PWCTL1		LOONGARCH_CSR_64(0x1d, 0)
#define KVM_CSR_STLBPS		LOONGARCH_CSR_64(0x1e, 0)
#define KVM_CSR_RVACFG		LOONGARCH_CSR_64(0x1f, 0)
#define KVM_CSR_CPUNUM		LOONGARCH_CSR_64(0x20, 0)
#define KVM_CSR_PRCFG1		LOONGARCH_CSR_64(0x21, 0)
#define KVM_CSR_PRCFG2		LOONGARCH_CSR_64(0x22, 0)
#define KVM_CSR_PRCFG3		LOONGARCH_CSR_64(0x23, 0)
#define KVM_CSR_KSCRATCH0	LOONGARCH_CSR_64(0x30, 0)
#define KVM_CSR_KSCRATCH1	LOONGARCH_CSR_64(0x31, 0)
#define KVM_CSR_KSCRATCH2	LOONGARCH_CSR_64(0x32, 0)
#define KVM_CSR_KSCRATCH3	LOONGARCH_CSR_64(0x33, 0)
#define KVM_CSR_KSCRATCH4	LOONGARCH_CSR_64(0x34, 0)
#define KVM_CSR_KSCRATCH5	LOONGARCH_CSR_64(0x35, 0)
#define KVM_CSR_KSCRATCH6	LOONGARCH_CSR_64(0x36, 0)
#define KVM_CSR_KSCRATCH7	LOONGARCH_CSR_64(0x37, 0)
#define KVM_CSR_KSCRATCH8	LOONGARCH_CSR_64(0x38, 0)
#define KVM_CSR_TIMERID		LOONGARCH_CSR_64(0x40, 0)
#define KVM_CSR_TIMERCFG	LOONGARCH_CSR_64(0x41, 0)
#define KVM_CSR_TIMERTICK	LOONGARCH_CSR_64(0x42, 0)
#define KVM_CSR_TIMEROFFSET	LOONGARCH_CSR_64(0x43, 0)
#define KVM_CSR_GSTAT		LOONGARCH_CSR_64(0x50, 0)
#define KVM_CSR_GCFG		LOONGARCH_CSR_64(0x51, 0)
#define KVM_CSR_GINTC		LOONGARCH_CSR_64(0x52, 0)
#define KVM_CSR_GCNTC		LOONGARCH_CSR_64(0x53, 0)
#define KVM_CSR_LLBCTL		LOONGARCH_CSR_64(0x60, 0)
#define KVM_CSR_IMPCTL1		LOONGARCH_CSR_64(0x80, 0)
#define KVM_CSR_IMPCTL2		LOONGARCH_CSR_64(0x81, 0)
#define KVM_CSR_GNMI		LOONGARCH_CSR_64(0x82, 0)
#define KVM_CSR_TLBREBASE	LOONGARCH_CSR_64(0x88, 0)
#define KVM_CSR_TLBRBADV	LOONGARCH_CSR_64(0x89, 0)
#define KVM_CSR_TLBREPC		LOONGARCH_CSR_64(0x8a, 0)
#define KVM_CSR_TLBRSAVE	LOONGARCH_CSR_64(0x8b, 0)
#define KVM_CSR_TLBRELO0	LOONGARCH_CSR_64(0x8c, 0)
#define KVM_CSR_TLBRELO1	LOONGARCH_CSR_64(0x8d, 0)
#define KVM_CSR_TLBREHI		LOONGARCH_CSR_64(0x8e, 0)
#define KVM_CSR_TLBRPRMD	LOONGARCH_CSR_64(0x8f, 0)
#define KVM_CSR_ERRCTL		LOONGARCH_CSR_64(0x90, 0)
#define KVM_CSR_ERRINFO1	LOONGARCH_CSR_64(0x91, 0)
#define KVM_CSR_ERRINFO2	LOONGARCH_CSR_64(0x92, 0)
#define KVM_CSR_ERREBASE	LOONGARCH_CSR_64(0x93, 0)
#define KVM_CSR_ERREPC		LOONGARCH_CSR_64(0x94, 0)
#define KVM_CSR_ERRSAVE		LOONGARCH_CSR_64(0x95, 0)
#define KVM_CSR_CTAG		LOONGARCH_CSR_64(0x98, 0)
#define KVM_CSR_MCSR0		LOONGARCH_CSR_64(0xc0, 0)
#define KVM_CSR_MCSR1		LOONGARCH_CSR_64(0xc1, 0)
#define KVM_CSR_MCSR2		LOONGARCH_CSR_64(0xc2, 0)
#define KVM_CSR_MCSR3		LOONGARCH_CSR_64(0xc3, 0)
#define KVM_CSR_MCSR8		LOONGARCH_CSR_64(0xc8, 0)
#define KVM_CSR_MCSR9		LOONGARCH_CSR_64(0xc9, 0)
#define KVM_CSR_MCSR10		LOONGARCH_CSR_64(0xca, 0)
#define KVM_CSR_MCSR24		LOONGARCH_CSR_64(0xf0, 0)
#define KVM_CSR_UCWIN		LOONGARCH_CSR_64(0x100, 0)
#define KVM_CSR_UCWIN0_LO	LOONGARCH_CSR_64(0x102, 0)
#define KVM_CSR_UCWIN0_HI	LOONGARCH_CSR_64(0x103, 0)
#define KVM_CSR_UCWIN1_LO	LOONGARCH_CSR_64(0x104, 0)
#define KVM_CSR_UCWIN1_HI	LOONGARCH_CSR_64(0x105, 0)
#define KVM_CSR_UCWIN2_LO	LOONGARCH_CSR_64(0x106, 0)
#define KVM_CSR_UCWIN2_HI	LOONGARCH_CSR_64(0x107, 0)
#define KVM_CSR_UCWIN3_LO	LOONGARCH_CSR_64(0x108, 0)
#define KVM_CSR_UCWIN3_HI	LOONGARCH_CSR_64(0x109, 0)
#define KVM_CSR_DMWIN0		LOONGARCH_CSR_64(0x180, 0)
#define KVM_CSR_DMWIN1		LOONGARCH_CSR_64(0x181, 0)
#define KVM_CSR_DMWIN2		LOONGARCH_CSR_64(0x182, 0)
#define KVM_CSR_DMWIN3		LOONGARCH_CSR_64(0x183, 0)
#define KVM_CSR_PERF0_EVENT	LOONGARCH_CSR_64(0x200, 0)
#define KVM_CSR_PERF0_COUNT	LOONGARCH_CSR_64(0x201, 0)
#define KVM_CSR_PERF1_EVENT	LOONGARCH_CSR_64(0x202, 0)
#define KVM_CSR_PERF1_COUNT	LOONGARCH_CSR_64(0x203, 0)
#define KVM_CSR_PERF2_EVENT	LOONGARCH_CSR_64(0x204, 0)
#define KVM_CSR_PERF2_COUNT	LOONGARCH_CSR_64(0x205, 0)
#define KVM_CSR_PERF3_EVENT	LOONGARCH_CSR_64(0x206, 0)
#define KVM_CSR_PERF3_COUNT	LOONGARCH_CSR_64(0x207, 0)
#define KVM_CSR_DEBUG		LOONGARCH_CSR_64(0x500, 0)
#define KVM_CSR_DEPC		LOONGARCH_CSR_64(0x501, 0)
#define KVM_CSR_DESAVE		LOONGARCH_CSR_64(0x502, 0)

// ring 0, disable interrupt
#define CRMD_PG 4
#define INIT_VALUE_CRMD (1 << CRMD_PG)
// TODO 感觉是否设置其实是无所谓的，唯一的例外就会 syscall
// 然后 syscall 马上会让这个数值覆盖
#define INIT_VALUE_PRMD 0x0
#define INIT_VALUE_EUEN 0x7
#define INIT_VALUE_MISC 0x0

// VS 入口 : 2 ** 3 是指令间距 看看内核对应的部分是怎么写的吧
#define INIT_VALUE_ECFG 0x70000

// 仅仅当 CSR.ECFG.VS 等于 0 的时候，这表示一个中断 ? 所以中断的入口在哪里啊 ?
// 参考 6.2.1，中断的 ecode = 等于中断号 + 64
// 
// TODO 软件中断和硬件中断位是什么意思？

// TODO era 不会记录 TLB refill, 那么 TLB refill 怎么回去呀 ?
// 7.4.10
#define INIT_VALUE_RVACFG 0x0

// TODO guest 可以直接访问 cpuid 吗?
#define INIT_VALUE_CPUID 0x0

// TODO 其实我一直都是非常的怀疑到底是否可以 SET 这些 config
// TODO 什么叫做 Timer 的有效位数 ?
#define INIT_VALUE_PRCFG1 0x72f8
// TLB 支持的页大小 [12, 29]
#define INIT_VALUE_PRCFG2 0x3ffff000
// TLB 的物理参数
#define INIT_VALUE_PRCFG3 0x8073f2

// TODO 低半地址空间和高半地址空间，为什么需要创建出来两个来 ?

// TODO 为什么需要保存 exception 之前的状态 和 返回地址
// TODO 为什么 TLB refill exception 需要单独保存 和 返回地址
// TODO TLBRERA / TLBRPRMD 按道理来说都是只读信息才对啊

// TODO section 5.3 直接映射 和 访问类型还是存在关系的 !

// TODO 关于各种调试寄存器之类的，还是需要小心的处理一下啊!
//
// 内核的疑惑:
// TODO 1. 按道理，应该是存在 K0 和 K1 这种寄存器

// TODO
// - [ ] 之前都是没有区分 STLB 和 MTLB 的，为什么可以正常工作的啊?
// - [ ] pagemask 的实现靠什么东西啊?


#define INIT_VALUE_STLBPS 0xE
#endif /* end of include guard: INTERNAL_H_6IUWCEFP */
