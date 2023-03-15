                MODULE   ?Reset_Handler

                EXTERN   Reset_Handler_C

                PUBWEAK  Reset_Handler
                SECTION  .text:CODE:REORDER:NOROOT(2)
Reset_Handler
                LDR      R0, =Reset_Handler_C
                BX       R0

                END
