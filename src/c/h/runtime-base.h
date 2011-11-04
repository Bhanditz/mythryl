// runtime-base.h


#ifndef RUNTIME_BASE_H
#define RUNTIME_BASE_H

// Macro concatenation (ANSI CPP)
//
#ifndef CONCAT /* assyntax.h also defines CONCAT */
    # define CONCAT(a,b)	a ## b
#endif
#define CONCAT3(a,b,c)	a ## b ## c

#define ONE_K_BINARY		1024
#define ONE_MEG_BINARY 	(ONE_K_BINARY*ONE_K_BINARY)

// The generated file
//
//     sizes-of-some-c-types--autogenerated.h
//
// defines various size-macros
// and the following types:
//
// Int16	-- 16-bit signed integer
// Int1	-- 32-bit signed integer
// Int2	-- 64-bit signed integer (64-bit machines only)
// Unt16	-- 16-bit unsigned integer
// Unt1	-- 32-bit unsigned integer
// Unt2	-- 64-bit unsigned integer (64-bit machines only)
// Unt8		-- Unsigned 8-bit integer.
// Val_Sized_Unt	-- Unsigned integer large enough for a Lib7 value.
// Val_Sized_Int	--   Signed integer large enough for a Lib7 value.
// Punt	-- Unsigned integer large enough for an address.
//
#include "sizes-of-some-c-types--autogenerated.h"


#define PAIR_BYTESIZE		(2*WORD_BYTESIZE)					// Size of a pair.
#define FLOAT64_SIZE_IN_WORDS		(FLOAT64_BYTESIZE / WORD_BYTESIZE)		// Val_Sized_Unt's per double.
#define PAIR_SIZE_IN_WORDS		2							// Val_Sized_Unt's per pair.
#define SPECIAL_CHUNK_SIZE_IN_WORDS	2							// Val_Sized_Unt's per special chunk.

// Convert a number of bytes
// to an even number of words:
//
#define BYTES_TO_WORDS(n)	(((n)+(WORD_BYTESIZE-1)) >> LOG2_BYTES_PER_WORD)

// Convert from double to word units:
//
#define DOUBLES_TO_WORDS(n)	((n) * FLOAT64_SIZE_IN_WORDS)

// On 32-bit machines it is useful to
// align doubles on 8-byte boundaries:
//
#ifndef SIZES_C_64_MYTHRYL_64
#  define ALIGN_FLOAT64S
#endif


#ifndef _ASM_

#include <stdlib.h>

typedef  Int1  Bool;

#ifndef TRUE		// Some systems already define TRUE and FALSE.
    #define TRUE  1
    #define FALSE 0
#endif

typedef Int1 Status;

#define SUCCESS 1
#define FAILURE 0

// Assertions for debugging:
//
#ifdef ASSERT_ON
    extern void AssertFail (const char *a, const char *file, int line);
//  #define ASSERT(A)	((A) ? ((void)0) : AssertFail(#A, __FILE__, __LINE__))
    #define ASSERT(A)	{ if (!(A)) AssertFail(#A, __FILE__, __LINE__); }
#else
    #define ASSERT(A)	{ }
#endif

// Convert a bigendian 32-bit quantity
// into the host machine's representation:
//
#if defined(BYTE_ORDER_BIG)
    //
    #define BIGENDIAN_TO_HOST(x)	(x)
    //
#elif defined(BYTE_ORDER_LITTLE)
    //
    extern Unt1 swap_word_bytes (Unt1 x);
    #define BIGENDIAN_TO_HOST(x)	swap_word_bytes(x)
    //
#else
    #error must define endian
#endif

// Round i up to the nearest multiple of n,
// where n is a power of 2
//
#define ROUND_UP_TO_POWER_OF_TWO(i, n)		(((i)+((n)-1)) & ~((n)-1))


// Extract the bitfield of width WIDTH
// starting at position POS from I:
//
#define XBITFIELD(I,POS,WIDTH)		(((I) >> (POS)) & ((1<<(WIDTH))-1))

// Aliases for malloc/free, so 
// that we can easily replace them:
//
#define MALLOC(size)	malloc(size)
#define _FREE		free
#define FREE(p)		_FREE(p)

#define MALLOC_CHUNK(t)	((t*)MALLOC(sizeof(t)))		// Allocate a new C chunk of type t.
#define MALLOC_VEC(t,n)	((t*)MALLOC((n)*sizeof(t)))	// Allocate a new C array of type t chunks.

#define CLEAR_MEMORY(m, size)	(memset((m), 0, (size)))

// C types used in the run-time system:
//
#ifdef SIZES_C_64_MYTHRYL_32
    //
    typedef Unt1  Val;
#else
    //
    typedef   struct { Val_Sized_Unt v[1]; }   Valchunk;	// Just something for a Val to point to.
    //
    typedef   Valchunk*   Val;					// Only place Valchunk type is used.
#endif
//
typedef struct pthread_state_struct	Pthread;		// struct pthread_state_struct	def in   src/c/h/pthread-state.h
typedef struct task			Task;			// struct task			def in   src/c/h/task.h 
typedef struct heap			Heap;			// struct heap			def in   src/c/h/heap.h


// System_Constant
//
// In C, system constants are usually integers.
// We represent these in the Mythryl system as
// (Int, String) pairs, where the integer is the
// C constant and the string is a short version
// of the symbolic name used in C (e.g., the constant
// EINTR might be represented as (4, "INTR")).
//
typedef struct {
    //
    int	   id;
    char*  name;
    //
} System_Constant;

// System_Constants_Table
//
typedef struct {
    //
    int		       constants_count;
    System_Constant*   consts;
    //
} System_Constants_Table;


// Run-time system messages:
//
extern void say       (char* fmt, ...);											// say				def in    src/c/main/error.c
extern void debug_say (char* fmt, ...);											// debug_say			def in    src/c/main/error.c
extern void say_error (char*,     ...);											// say_error			def in    src/c/main/error.c
extern void die       (char*,     ...);											// die				def in    src/c/main/error.c

extern void print_stats_and_exit      (int code);									// print_stats_and_exit		def in    src/c/main/runtime-main.c

typedef   struct cleaner_args   Cleaner_Args;
    //
    // An abstract type whose representation depends
    // on the particular cleaner being used.

extern Cleaner_Args*   handle_cleaner_commandline_arguments   (char** argv);						// handle_cleaner_commandline_arguments	def in   src/c/heapcleaner/heapcleaner-initialization.c

extern Task* make_task               (Bool is_boot, Cleaner_Args* params);						// make_task			def in   src/c/main/runtime-state.c
extern void  load_compiled_files  (const char* compiled_files_to_load_filename, Cleaner_Args* params);			// load_compiled_files		def in   src/c/main/load-compiledfiles.c/load_compiled_files()
extern void  load_and_run_heap_image (const char* heap_image_to_run_filename,         Cleaner_Args* params);		// load_and_run_heap_image	def in   src/c/main/load-and-run-heap-image.c

extern void initialize_task (Task *task);										// initialize_task		def in   src/c/main/runtime-state.c
extern void save_c_state    (Task *task, ...);										// save_c_state			def in   src/c/main/runtime-state.c
extern void restore_c_state (Task *task, ...);										// restore_c_state		def in   src/c/main/runtime-state.c

extern void set_up_timers ();

extern Val    run_mythryl_function (Task *task, Val f, Val arg, Bool use_fate);					// run_mythryl_function		def in   src/c/main/run-mythryl-code-and-runtime-eventloop.c

extern void   reset_timers (Pthread* pthread);
extern void   run_mythryl_task_and_runtime_eventloop (Task* task);						// run_mythryl_task_and_runtime_eventloop	def in   src/c/main/run-mythryl-code-and-runtime-eventloop.c
extern void   raise_mythryl_exception (Task* task, Val exn);							// raise_mythryl_exception			def in   src/c/main/run-mythryl-code-and-runtime-eventloop.c
extern void   handle_uncaught_exception   (Val e);								// handle_uncaught_exception			def in   src/c/main/runtime-exception-stuff.c

extern void   set_up_fault_handlers ();										// set_up_fault_handlers		def in   src/c/machine-dependent/posix-arithmetic-trap-handlers.c
														// set_up_fault_handlers		def in   src/c/machine-dependent/cygwin-fault.c
														// set_up_fault_handlers		def in   src/c/machine-dependent/win32-fault.c
#if NEED_SOFTWARE_GENERATED_PERIODIC_EVENTS
    //
    extern void reset_heap_allocation_limit_for_software_generated_periodic_events (Task *task);
#endif


// These are two views of the command line arguments.
// raw_args is essentially argv[].
// commandline_arguments is argv[] with runtime system arguments stripped
// out (e.g., those of the form --runtime-xxx[=yyy]).
//
extern char** raw_args;
extern char** commandline_arguments;				// Does not include the command name (argv[0]).
extern char*  mythryl_program_name__global;			// Command name used to invoke the runtime.
extern int    verbosity;
extern Bool   codechunk_comment_display_is_enabled__global;	// Set per   --show-code-chunk-comments	  commandline switch in   src/c/main/runtime-main.c
extern Bool   cleaner_messages_are_enabled__global;		// Set                                                       in   src/c/lib/heap/heapcleaner-control.c
extern Bool   unlimited_heap_is_enabled__global;			// Set per   --unlimited-heap             commandline switch in   src/c/heapcleaner/heapcleaner-initialization.c

extern Pthread*	pthread_table__global [];

extern int   pth__done_acquire_pthread__global;
    //
    // This boolean flag starts out FALSE and is set TRUE
    // the first time   pth__acquire_pthread   is called.
    //
    // We can use simple mutex-free monothread logic in
    // the heapcleaner (etc) so long as this is FALSE.



#endif // _ASM_ 

#ifndef HEAP_IMAGE_SYMBOL
#define HEAP_IMAGE_SYMBOL       "lib7_heap_image"
#define HEAP_IMAGE_LEN_SYMBOL   "lib7_heap_image_len"
#endif

#endif // RUNTIME_BASE_H


// COPYRIGHT (c) 1992 AT&T Bell Laboratories
// Subsequent changes by Jeff Prothero Copyright (c) 2010-2011,
// released under Gnu Public Licence version 3.


