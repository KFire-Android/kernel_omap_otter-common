#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpuidle

#if !defined(_TRACE_CPUIDLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CPUIDLE_H

#include <linux/atomic.h>
#include <linux/tracepoint.h>

extern atomic_t cpuidle_trace_seq;

TRACE_EVENT(coupled_enter,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_exit,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_spin,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_unspin,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_safe_enter,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_safe_exit,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_idle_enter,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_idle_exit,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_abort,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_detected_abort,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_poke,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

TRACE_EVENT(coupled_poked,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, seq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->seq = atomic_inc_return(&cpuidle_trace_seq);
	),

	TP_printk("%u %u", __entry->seq, __entry->cpu)
);

#endif /* if !defined(_TRACE_CPUIDLE_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
