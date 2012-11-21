// posix-signal.c
//
// Posix-specific code to support Mythryl-level handling of posix signals.


#include "../mythryl-config.h"

#include <stdio.h>
#include <stdlib.h>

#include "system-dependent-unix-stuff.h"
#include "system-dependent-signal-get-set-etc.h"
#include "runtime-base.h"
#include "runtime-configuration.h"
#include "make-strings-and-vectors-etc.h"
#include "system-dependent-signal-stuff.h"
#include "system-signals.h"
#include "runtime-globals.h"
#include "heap.h"



// The generated System_Constant table for UNIX signals:
//
#include "posix-signal-table--autogenerated.c"



// SELF_HOSTTHREAD is used in
//
//     arithmetic_fault_handler					// arithmetic_fault_handler	def in   src/c/machine-dependent/posix-arithmetic-trap-handlers.c
//
// to supply the SELF_HOSTTHREAD->task
// and           SELF_HOSTTHREAD->executing_mythryl_code
// values for handling
// a divide-by-zero or whatever.
//
// This is probably pretty BOGUS ON LINUX -- I think
// it means a divide-by-zero in any thread will always
// be reported as being in thread zero.
//
// (Can't we just map our pid to our hostthread_table__global entry,
// by linear scan if nothing else?  -- 2011-11-03 CrT)
// 
// 
#define SELF_HOSTTHREAD	(pth__get_hostthread_by_ptid( pth__get_hostthread_ptid() ))			// Note that we still have   #define SELF_HOSTTHREAD	(hostthread_table__global[ 0 ])   in   src/c/machine-dependent/posix-arithmetic-trap-handlers.c


#ifdef USE_ZERO_LIMIT_PTR_FN
Punt		SavedPC;
extern		Zero_Heap_Allocation_Limit[];								// Actually a pointer, not an array.
#endif


static void   c_signal_handler   (/* int sig,  Signal_Handler_Info_Arg info,  Signal_Handler_Context_Arg* scp */);


Val   list_signals__may_heapclean   (Task* task, Roots* extra_roots)   {				// Called from src/c/lib/signal/listsignals.c
    //===========================
    //
    return dump_table_as_system_constants_list__may_heapclean (task, &SigTable, extra_roots);		// See src/c/heapcleaner/make-strings-and-vectors-etc.c
}

void   pause_until_signal   (Hostthread* hostthread) {
    // ==================
    //
    // Suspend the given Hostthread
    // until a signal is received:
    pause ();												// pause() is a clib function, see pause(2).
}

void   set_signal_state   (Hostthread* hostthread,  int sig_num,  int signal_state) {			// This fn is called (only) from   src/c/lib/signal/setsigstate.c
    // ================
    //
    // QUESTIONS:
    //
    // If we disable a signal that has pending signals,
    // should the pending signals be discarded?
    //
    // How do we keep track of the state
    // of non-UNIX signals (e.g., HEAPCLEANING_DONE)?				XXX BUGGO FIXME -- maybe should be a dedicated refcell in heap image for each one, although preserving their state across dump/load seems a minor concern.


    switch (sig_num) {
	//
    case RUNSIG_HEAPCLEANING_DONE:
        //
	hostthread->heapcleaning_done_signal_handler_state =  signal_state;
	break;

    case RUNSIG_THREAD_SCHEDULER_TIMESLICE:
        //
	hostthread->thread_scheduler_timeslice_signal_handler_state =  signal_state;
	break;

    default:
	if (IS_SYSTEM_SIG(sig_num)) {
	    //
	    switch (signal_state) {
	        //
	    case LIB7_SIG_IGNORE:
		SELECT_SIG_IGN_HANDLING_FOR_SIGNAL (sig_num);
		break;

	    case LIB7_SIG_DEFAULT:
		SELECT_SIG_DFL_HANDLING_FOR_SIGNAL( sig_num );
		break;

	    case LIB7_SIG_ENABLED:
		SET_SIGNAL_HANDLER( sig_num, c_signal_handler );					// SET_SIGNAL_HANDLER 	#define in   src/c/machine-dependent/system-dependent-signal-get-set-etc.h
		break;

	    default:
		die ("bogus signal state: sig = %d, state = %d\n",
		    sig_num, signal_state);
	    }

	} else {

            die ("set_signal_state: unknown signal %d\n", sig_num);
	}
    }
}


int   get_signal_state   (Hostthread* hostthread,  int sig_num)   {
    //================
    //
    switch (sig_num) {
        //
    case RUNSIG_HEAPCLEANING_DONE:
        //
	return hostthread->heapcleaning_done_signal_handler_state;

    case RUNSIG_THREAD_SCHEDULER_TIMESLICE:
        //
	return hostthread->thread_scheduler_timeslice_signal_handler_state;

    default:
	if (!IS_SYSTEM_SIG(sig_num))   die ("get_signal_state: unknown signal %d\n", sig_num);
	//
        {   void    (*handler)();
	    //
	    GET_SIGNAL_HANDLER( sig_num, handler );				// Store it into 'handler'.
	    //
	    if      (handler == SIG_IGN)	return LIB7_SIG_IGNORE;
	    else if (handler == SIG_DFL)	return LIB7_SIG_DEFAULT;
	    else				return LIB7_SIG_ENABLED;
	}
    }
}


#if defined(HAS_POSIX_SIGS) && defined(HAS_UCONTEXT)

// In this case    src/c/h/system-dependent-signal-get-set-etc.h
// set c_signal_handler up to be registered as a signal handler
// via sigaction with the SA_SIGINFO flag set, which according to
// man sigaction(2) means:
//
//     If  SA_SIGINFO  is  specified in sa_flags, then sa_sigaction (instead
//     of sa_handler) specifies the signal-handling function for signum.
//     This function receives the signal number as its first argument,
//     a pointer to a siginfo_t as its second argument and
//     a pointer to a ucontext_t (cast to void *) as its third argument.
//
// where
//     The siginfo_t argument to sa_sigaction is a struct with the following elements:
//
//         siginfo_t {
//             int      si_signo;    /* Signal number */
//             int      si_errno;    /* An errno value */
//             int      si_code;     /* Signal code */
//             int      si_trapno;   /* Trap number that caused hardware-generated signal (unused on most architectures) */
//             pid_t    si_pid;      /* Sending process ID */
//             uid_t    si_uid;      /* Real user ID of sending process */
//             int      si_status;   /* Exit value or signal */
//             clock_t  si_utime;    /* User time consumed */
//             clock_t  si_stime;    /* System time consumed */
//             sigval_t si_value;    /* Signal value */
//             int      si_int;      /* POSIX.1b signal */
//             void    *si_ptr;      /* POSIX.1b signal */
//             int      si_overrun;  /* Timer overrun count; POSIX.1b timers */
//             int      si_timerid;  /* Timer ID; POSIX.1b timers */
//             void    *si_addr;     /* Memory location which caused fault */
//             long     si_band;     /* Band event (was int in glibc 2.3.2 and earlier) */
//             int      si_fd;       /* File descriptor */
//             short    si_addr_lsb; /* Least significant bit of address since kernel 2.6.32) */
//           }
//
// ucontext_t is defined in <ucontext.h>
// man getcontext(2) says:


static int milliseconds_between_ramlog_and_syslog_dumps = -1;		// -1 == not initialized, 0 == don't dump.
static int milliseconds_since_last_ramlog_and_syslog_dump = 0;

static void   c_signal_handler   (int sig,  siginfo_t* si,  void* c)   {
    //        ================
    //
    // This is the C signal handler for
    // signals that are to be passed to
    // the Mythryl level via signal_handler in
    //
    //     src/lib/std/src/nj/runtime-signals-guts.pkg
    //

    ucontext_t* scp		/* This variable is unused on some platforms, so suppress 'unused var' compiler warning: */   __attribute__((unused))
        =
        (ucontext_t*) c;

    Hostthread* hostthread = SELF_HOSTTHREAD;

// Commented out because repeat entries were flooding the syscall_log.
// Should either include a dup count or just ignore consecutive
// identical syscall_log entries.
										Task* task =  hostthread->task;
//										ENTER_MYTHRYL_CALLABLE_C_FN(__func__);


    // Sanity check:  We compile in a MAX_POSIX_SIGNALS value but
    // have no way to ensure that we don't wind up getting run
    // on some custom kernel supporting more than MAX_POSIX_SIGNAL,
    // so we check here to be safe:
    //
    if (sig >= MAX_POSIX_SIGNALS)    die ("posix-signal.c: c_signal_handler: sig d=%d >= MAX_POSIX_SIGNAL %d\n", sig, MAX_POSIX_SIGNALS ); 



    /////////////////////////////////// begin kludge ////////////////////////////////////
    // This is a little kludge because I'm getting unexpected compiler lockups
    // during the serial -> concurrent programming paradigm transition, and I'd
    // like to look at the ramlog and syscall log but attaching gdb doesn't work.
    // (It may be confused by our 8K Mythryl stackframe.)  The idea is just to
    // dump the syscall log and ramlog every five seconds or so, which should be
    // frequent enough to quell impatience but infrequent enough not to add
    // significant overhead to compiles:  -- 2012-10-15 CrT
    //
    // Upgraded to allow control via the environment var
    //     MILLISECONDS_BETWEEN_RAMLOG_AND_SYSLOG_DUMPS
    //   -- 2012-10-18 CrT
    //
    if (milliseconds_between_ramlog_and_syslog_dumps < 0) {
	//
	char* t;
	if (!(t = getenv("MILLISECONDS_BETWEEN_RAMLOG_AND_SYSLOG_DUMPS"))) {
	    //
	    milliseconds_between_ramlog_and_syslog_dumps = 0;					// Env var not set, so let's not do this stuff.
	} else {
	    int ms = atoi(t);
	    if (ms < 0)   	    milliseconds_between_ramlog_and_syslog_dumps = 0;
	    else if (ms < 100)	    milliseconds_between_ramlog_and_syslog_dumps = 100;		// Let's not try dumps every millisecond.
	    else		    milliseconds_between_ramlog_and_syslog_dumps = ms;
	}
    }
    if (milliseconds_between_ramlog_and_syslog_dumps > 0) {
	//
	milliseconds_since_last_ramlog_and_syslog_dump += 20;					// I'm assuming the usual 50Hz SIGALRM. Yes, this is fragile and hacky,
												// but this is just a nonce debugging hack anyhow. Feel free to improve it.
	if (milliseconds_since_last_ramlog_and_syslog_dump > milliseconds_between_ramlog_and_syslog_dumps) {
	    milliseconds_since_last_ramlog_and_syslog_dump = 0;
	    //
	    dump_ramlog     (task,"c_signal_handler");
	    dump_syscall_log(task,"c_signal_handler");
	}
    }
    //
    ///////////////////////////////////   end kludge ////////////////////////////////////



    // Remember that we have seen signal number 'sig'.
    //
    // This will eventually get noticed by  choose_signal()  in
    //
    //     src/c/machine-dependent/signal-stuff.c
    //
    hostthread->posix_signal_counts[sig].seen_count++;
    hostthread->all_posix_signals.seen_count++;

//    log_if(
//        "posix-signal.c/c_signal_handler: signal d=%d  seen_count d=%d  done_count d=%d   diff d=%d",
//        sig,
//        hostthread->posix_signal_counts[sig].seen_count,
//        hostthread->posix_signal_counts[sig].done_count,
//        hostthread->posix_signal_counts[sig].seen_count - hostthread->posix_signal_counts[sig].done_count
//    );

    #ifdef SIGNAL_DEBUG
    debug_say ("c_signal_handler: sig = %d, pending = %d, inHandler = %d\n", sig, hostthread->posix_signal_pending, hostthread->mythryl_handler_for_posix_signal_is_running);
    #endif

    // The following line is needed only when
    // currently executing "pure" C code, but
    // doing it anyway in all other cases will
    // not hurt:
    //
    hostthread->ccall_limit_pointer_mask = 0;

    if (  hostthread->executing_mythryl_code
    &&  ! hostthread->posix_signal_pending
    &&  ! hostthread->mythryl_handler_for_posix_signal_is_running
    ){
	hostthread->posix_signal_pending = TRUE;

	#ifdef USE_ZERO_LIMIT_PTR_FN
	    //									// We don't use this approach currently; if we start using it again we need to check for possiblity of different hostthreads clobbering shared global storage. -- 2012-10-11 CrT
	    SIG_SavePC( hostthread->task, scp );
	    SET_SIGNAL_PROGRAM_COUNTER( scp,  );
	#else
	    ZERO_HEAP_ALLOCATION_LIMIT_FROM_C_SIGNAL_HANDLER( scp );			// OK to adjust the heap limit directly.
	#endif
    }
//										EXIT_MYTHRYL_CALLABLE_C_FN(__func__);
}

#else

static void   c_signal_handler   (
    //
    int		    sig,
    #if (defined(TARGET_PWRPC32) && defined(OPSYS_LINUX))
	Signal_Handler_Context_Arg*   scp
    #else
	Signal_Handler_Info_Arg	info,
	Signal_Handler_Context_Arg*   scp
    #endif
){
    #if defined(OPSYS_LINUX)  &&  defined(TARGET_INTEL32)  &&  defined(USE_ZERO_LIMIT_PTR_FN)
	//
	Signal_Handler_Context_Arg*  scp =  &sc;			// I find no trace of 'sc'; this looks like some ancient hack mostly bitrotted away.  -- 2012-10-11 CrT
    #endif

    Hostthread*  hostthread =  SELF_HOSTTHREAD;
    Task* task =  hostthread->task;
									    ENTER_MYTHRYL_CALLABLE_C_FN(__func__);

    hostthread->posix_signal_counts[sig].seen_count++;
    hostthread->all_posix_signals.seen_count++;

    #ifdef SIGNAL_DEBUG
    debug_say ("c_signal_handler: sig = %d, pending = %d, inHandler = %d\n",
    sig, hostthread->posix_signal_pending, hostthread->mythryl_handler_for_posix_signal_is_running);
    #endif

    // The following line is needed only when
    // currently executing "pure" C code, but
    // doing it anyway in all other cases will
    // not hurt:
    //
    hostthread->ccall_limit_pointer_mask = 0;

    if (  hostthread-> executing_mythryl_code
    && (! hostthread-> posix_signal_pending)
    && (! hostthread-> mythryl_handler_for_posix_signal_is_running)
    ){
        //
	hostthread->posix_signal_pending =  TRUE;

	#ifdef USE_ZERO_LIMIT_PTR_FN	
	    //									// We don't use this approach currently; if we start using it again we need to check for possiblity of different hostthreads clobbering shared global storage. -- 2012-10-11 CrT
	    SIG_SavePC( hostthread->task, scp );
	    SET_SIGNAL_PROGRAM_COUNTER( scp, Zero_Heap_Allocation_Limit );
	#else
	    ZERO_HEAP_ALLOCATION_LIMIT_FROM_C_SIGNAL_HANDLER( scp );		// OK to adjust the heap limit directly.
	#endif
    }
									    EXIT_MYTHRYL_CALLABLE_C_FN(__func__);
}

#endif


void   set_signal_mask   (Task* task, Val arg)   {
    // ===============
    // 
    // Set the signal mask to the list of signals given by 'arg'.
    // The signal_list has the type
    //
    //     "sysconst list option"
    //
    // with the following semantics -- see src/lib/std/src/nj/runtime-signals.pkg
    //
    //	NULL	-- the empty mask
    //	THE[]	-- mask all signals
    //	THE l	-- the signals in l are the mask
    //

  //									    ENTER_MYTHRYL_CALLABLE_C_FN(__func__);

    Signal_Set	mask;											// Signal_Set		is from   src/c/h/system-dependent-signal-get-set-etc.h
    int		i;

    CLEAR_SIGNAL_SET(mask);										// CLEAR_SIGNAL_SET	is from   src/c/h/system-dependent-signal-get-set-etc.h

    Val signal_list  = arg;
    if (signal_list != OPTION_NULL) {
	signal_list  = OPTION_GET(signal_list);

	if (LIST_IS_NULL(signal_list)) {
	    //
	    // THE [] -- mask all signals
            //
	    for (i = 0;  i < NUM_SYSTEM_SIGS;  i++) {
	        //
		ADD_SIGNAL_TO_SET( mask, SigInfo[i].id );						// ADD_SIGNAL_TO_SET	is from   src/c/h/system-dependent-signal-get-set-etc.h
	    }

	} else {

	    while (signal_list != LIST_NIL) {
		Val	car = LIST_HEAD(signal_list);
		int	sig = GET_TUPLE_SLOT_AS_INT(car, 0);
		ADD_SIGNAL_TO_SET(mask, sig);
		signal_list = LIST_TAIL(signal_list);
	    }
	}
    }

    // Do the actual host OS syscall
    // to change the signal mask.
    // This is our only invocation of this syscall:
    //
//  log_if("posix-signal.c/set_signal_mask: setting host signal mask for process to x=%x", mask );	// Commented out because it floods mythryl.compile.log -- 2011-10-10 CrT
    //
    RELEASE_MYTHRYL_HEAP( task->hostthread, "set_signal_mask", NULL );
	//
	SET_PROCESS_SIGNAL_MASK( mask );
	//
    RECOVER_MYTHRYL_HEAP( task->hostthread, "set_signal_mask" );
    //									    EXIT_MYTHRYL_CALLABLE_C_FN(__func__);
}


Val   get_signal_mask__may_heapclean   (Task* task, Val arg, Roots* extra_roots)   {		// Called from src/c/lib/signal/getsigmask.c
    //==============================
    // 
    // 'arg' is unused.
    // 
    // Return the current signal mask (only those signals supported by Mythryl);
    // like set_signal_mask, the result has the following semantics:
    //	NULL	-- the empty mask
    //	THE[]	-- mask all signals
    //	THE l	-- the signals in l are the mask

  //									    ENTER_MYTHRYL_CALLABLE_C_FN(__func__);

    Signal_Set	mask;
    int		i;
    int		n;

    RELEASE_MYTHRYL_HEAP( task->hostthread, "_lib7_Sig_getsigmask", NULL );
	//
	GET_PROCESS_SIGNAL_MASK( mask );
	//
	// Count the number of masked signals:
	//
	for (i = 0, n = 0;  i < NUM_SYSTEM_SIGS;  i++) {
	    //
	    if (SIGNAL_IS_IN_SET(mask, SigInfo[i].id))   n++;
	}
	//
    RECOVER_MYTHRYL_HEAP( task->hostthread, "_lib7_Sig_getsigmask" );

    if (n == 0)   return OPTION_NULL;

    Val	signal_list;

    if (n == NUM_SYSTEM_SIGS) {
	//
	signal_list = LIST_NIL;
	//
    } else {
	//
        Roots roots1 = { &signal_list, extra_roots };
	//
	for (i = 0, signal_list = LIST_NIL;   i < NUM_SYSTEM_SIGS;   i++) {

	    // If our agegroup0 buffer is more than half full,
	    // empty it by doing a heapcleaning.  This is very
	    // conservative -- which is the way I like it. :-)
	    //
	    if (agegroup0_freespace_in_bytes( task )
	      < agegroup0_usedspace_in_bytes( task )
	    ){
		call_heapcleaner_with_extra_roots( task,  0, &roots1 );
	    }

	    if (SIGNAL_IS_IN_SET(mask, SigInfo[i].id)) {
	        //
		Val name =  make_ascii_string_from_c_string__may_heapclean (task, SigInfo[i].name, &roots1 );

		Val sig  =  make_two_slot_record( task, TAGGED_INT_FROM_C_INT(SigInfo[i].id), name);

		signal_list = LIST_CONS(task, sig, signal_list);
	    }
	}
    }

    Val result =  OPTION_THE( task, signal_list );
									    EXIT_MYTHRYL_CALLABLE_C_FN(__func__);
    return result;
}


// COPYRIGHT (c) 1992 by AT&T Bell Laboratories.
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2012,
// released under Gnu Public Licence version 3.


