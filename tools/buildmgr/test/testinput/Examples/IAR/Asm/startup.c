
/*----------------------------------------------------------------------------
   References
 *----------------------------------------------------------------------------*/
extern unsigned int CSTACK$$Limit;
extern int main(void);
extern void Reset_Handler  (void);
void Reset_Handler_C(void);
void Default_Handler(void);

/*----------------------------------------------------------------------------
  Exception / Interrupt Handler
 *----------------------------------------------------------------------------*/
void RESERVED_Handler                             (void) __attribute__ ((weak, alias("Default_Handler")));

/* Exceptions */
void NMI_Handler                                  (void) __attribute__ ((weak, alias("Default_Handler")));
void HardFault_Handler                            (void) __attribute__ ((weak));
void MemManage_Handler                            (void) __attribute__ ((weak, alias("Default_Handler")));
void BusFault_Handler                             (void) __attribute__ ((weak, alias("Default_Handler")));
void UsageFault_Handler                           (void) __attribute__ ((weak, alias("Default_Handler")));
void SVC_Handler                                  (void) __attribute__ ((weak, alias("Default_Handler")));
void DebugMon_Handler                             (void) __attribute__ ((weak, alias("Default_Handler")));
void PendSV_Handler                               (void) __attribute__ ((weak, alias("Default_Handler")));
void SysTick_Handler                              (void) __attribute__ ((weak, alias("Default_Handler")));

  /* Interrupts */
void INT0_Handler                                 (void) __attribute__ ((weak, alias("Default_Handler")));
void INT1_Handler                                 (void) __attribute__ ((weak, alias("Default_Handler")));
void INT2_Handler                                 (void) __attribute__ ((weak, alias("Default_Handler")));
void INT3_Handler                                 (void) __attribute__ ((weak, alias("Default_Handler")));


/*----------------------------------------------------------------------------
  Exception / Interrupt Vector table
 *----------------------------------------------------------------------------*/

#if defined ( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef void(*VECTOR_TABLE_Type)(void);

extern const VECTOR_TABLE_Type __vector_table[256];
       const VECTOR_TABLE_Type __vector_table[256] @".intvec" = {
  (VECTOR_TABLE_Type)(&CSTACK$$Limit),           /*     Initial Stack Pointer */
  Reset_Handler,                                 /*     Reset Handler */
  NMI_Handler,                                   /* -14 NMI Handler */
  HardFault_Handler,                             /* -13 Hard Fault Handler */
  MemManage_Handler,                             /* -12 MPU Fault Handler */
  BusFault_Handler,                              /* -11 Bus Fault Handler */
  UsageFault_Handler,                            /* -10 Usage Fault Handler */
  RESERVED_Handler,                              /*     Reserved */
  RESERVED_Handler,                              /*     Reserved */
  RESERVED_Handler,                              /*     Reserved */
  RESERVED_Handler,                              /*     Reserved */
  SVC_Handler,                                   /*  -5 SVCall Handler */
  DebugMon_Handler,                              /*  -4 Debug Monitor Handler */
  RESERVED_Handler,                              /*     Reserved */
  PendSV_Handler,                                /*  -2 PendSV Handler */
  SysTick_Handler,                               /*  -1 SysTick Handler */

  /* Interrupts */
  INT0_Handler,                                  /*   0 CPU to CPU int0 */
  INT1_Handler,                                  /*   1 CPU to CPU int1 */
  INT2_Handler,                                  /*   2 CPU to CPU int2 */
  INT3_Handler                                   /*   3 CPU to CPU int3 */
};

#if defined ( __GNUC__ )
#pragma GCC diagnostic pop
#endif

/*----------------------------------------------------------------------------
  Reset Handler called on controller reset
 *----------------------------------------------------------------------------*/
void Reset_Handler_C(void)
{
  main();
  while(1);
}


#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif

/*----------------------------------------------------------------------------
  Hard Fault Handler
 *----------------------------------------------------------------------------*/
void HardFault_Handler(void)
{
  while(1);
}

/*----------------------------------------------------------------------------
  Default Handler for Exceptions / Interrupts
 *----------------------------------------------------------------------------*/
void Default_Handler(void)
{
  while(1);
}

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic pop
#endif

