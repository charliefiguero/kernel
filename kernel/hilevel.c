/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"
#define maximum_number_of_PCBs 16
#define size_of_stack 0x00001000

int number_of_pcbs = 1;
pcb_t pcb[ maximum_number_of_PCBs ]; pcb_t* current = NULL;

// pcb_t* largest_process = NULL; //Used for scheduling

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

    PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );

    current = next;                             // update   executing index   to P_{next}

  return;
}


//TODO:
//dont schedule terminated programs
//deallocate terminated programs

void schedule( ctx_t* ctx ) {

  //Increment age of PCBs
  for (int i = 0; i < number_of_pcbs; i++){
      pcb[ i ].age += pcb[ i ].priority;
  }
  //Reset age of current PCB
  current->age = 0;

  //Find PCB with greatest age + priority
  pcb_t* largest_process = &pcb[ 0 ];
  int process_size = 0;

  for (int i = 0; i < number_of_pcbs; i++){
    if(pcb[ i ].status != STATUS_TERMINATED){
      int s = pcb[ i ].priority + pcb[ i ].age;
      if (s >= process_size){
        largest_process = &pcb[ i ];
        process_size = s;
      }
    }
  }

  //Dispatch PCB with greatest age + priority
  dispatch(ctx, current, largest_process);

  // if     ( current->pid == pcb[ 0 ].pid ) {
  //   dispatch( ctx, &pcb[ 0 ], &pcb[ 1 ] );      // context switch P_1 -> P_2
  //
  //   pcb[ 0 ].status = STATUS_READY;             // update   execution status  of P_1
  //   pcb[ 1 ].status = STATUS_EXECUTING;         // update   execution status  of P_2
  // }
  // else if( current->pid == pcb[ 1 ].pid ) {
  //   dispatch( ctx, &pcb[ 1 ], &pcb[ 0 ] );      // context switch P_2 -> P_1
  //
  //   pcb[ 1 ].status = STATUS_READY;             // update   execution status  of P_2
  //   pcb[ 0 ].status = STATUS_EXECUTING;         // update   execution status  of P_1
  // }

  return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void     main_console();
extern uint32_t tos_fatty_boi;
// extern void     main_P3();
// extern uint32_t tos_P3;
// extern void     main_P4();
// extern uint32_t tos_P4;

void hilevel_handler_rst( ctx_t* ctx              ) {
  /* Initialise two PCBs, representing user processes stemming from execution
   * of two user programs.  Note in each case that
   *
   * - the CPSR value of 0x50 means the processor is switched into USR mode,
   *   with IRQ interrupts enabled, and
   * - the PC and SP values matche the entry point and top of stack.
   */

  PL011_putc( UART0, 'R',      true ); //Debugging check

  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );     // initialise 0-th PCB = P_1
  pcb[ 0 ].pid      = 1;
  pcb[ 0 ].status   = STATUS_CREATED;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_fatty_boi );
  pcb[ 0 ].priority = 2;
  pcb[ 0 ].age = 0;

  //set tos for all possible PCBs
  for (int i = 0; i < maximum_number_of_PCBs; i++){
    pcb[ i ].tos = (uint32_t) (&tos_fatty_boi - (i * 0x00001000));
    PL011_putc( UART0, 'F',      true );
        // pcb[ i ].tos = tos_fatty_boi - (i * 0x00001000);
  }

  /* Once the PCBs are initialised, we arbitrarily select the one in the 0-th
   * PCB to be executed: there is no need to preserve the execution context,
   * since it is is invalid on reset (i.e., no process will previously have
   * been executing).
   */
   /* Configure the mechanism for interrupt handling by
    *
    * - configuring timer st. it raises a (periodic) interrupt for each
    *   timer tick,
    * - configuring GIC st. the selected interrupts are forwarded to the
    *   processor via the IRQ interrupt signal, then
    * - enabling IRQ interrupts.
    */

   TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
   TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
   TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
   TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
   TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

   GICC0->PMR          = 0x000000F0; // unmask all            interrupts
   GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
   GICC0->CTLR         = 0x00000001; // enable GIC interface
   GICD0->CTLR         = 0x00000001; // enable GIC distributor

  dispatch( ctx, NULL, &pcb[ 0 ] );
  int_enable_irq();

  return;
}

void hilevel_handler_irq( ctx_t* ctx ) {
  // Step 2: read  the interrupt identifier so we know the source.
  uint32_t id = GICC0->IAR;
  // Step 4: handle the interrupt, then clear (or reset) the source.
  if( id == GIC_SOURCE_TIMER0 ) {
    // PL011_putc( UART0, 'T', true );
    TIMER0->Timer1IntClr = 0x01;
    schedule(ctx);
  }
  // Step 5: write the interrupt identifier to signal we're done.
  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {
  /* Based on the identifier (i.e., the immediate operand) extracted from the
   * svc instruction,
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call, then
   * - write any return value back to preserved usr mode registers.
   */

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      schedule( ctx );

      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;

      break;
    }
    //void *memcpy(void *destination, const void *source, size_t n)

    case 0x03 : { // 0x03 => fork()
      number_of_pcbs += 1;
      int new_pcb = number_of_pcbs - 1;
      // memset( &pcb[ new_pcb ], 0, sizeof( pcb_t ) );     // initialise 0-th PCB = P_1

      pcb[ new_pcb ].pid    = number_of_pcbs;
      memcpy(&pcb[ new_pcb ].ctx, ctx, sizeof(ctx_t)); //memcpy cpu status
      pcb[ new_pcb ].ctx.pc = ctx->pc; //update child's program counter

      int offset = current->tos - ctx->sp;                        // find offset between parent's tos and sp
      pcb[ new_pcb ].ctx.sp = pcb[ new_pcb ].tos - offset;        //update child's stack pointer
      memcpy( (void*)(pcb[ new_pcb ].tos - size_of_stack), (void*)(current->tos - size_of_stack), size_of_stack);          // memcpy parent's used stack to child's stack

      pcb[ new_pcb ].priority = 1;
      pcb[ new_pcb ].age = 0;
      pcb[ new_pcb ].status   = STATUS_CREATED;

      //if child return 0, if parent return id of child
      pcb[ new_pcb ].ctx.gpr[0] = 0;
      ctx->gpr[0] = pcb[ new_pcb ].pid;

      break;
    }

    case 0x04 : { // 0x04 => exit( int x )
      current->status = STATUS_TERMINATED;
      schedule( ctx );
      break;
    }

    case 0x05 : { // 0x05 => exec( int x )
      ctx->pc   = ( uint32_t )( ctx->gpr[ 0 ] );
      ctx->sp = current->tos;
      PL011_putc( UART0, 'E',      true ); //Debugging check
        // schedule( ctx ); NEVER UNCOMMENT. BEWARE JLO CODE
      break;
    }

    case 0x06 : { // 0x06 => kill( pid_t pid, int x )
      pid_t given_pid = ( pid_t )( ctx->gpr[ 0 ] );
      pcb[ given_pid - 1 ].status = STATUS_TERMINATED;
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
