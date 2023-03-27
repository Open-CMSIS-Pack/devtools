                MODULE SimpleTestAsm
                PUBLIC Asm_Func
                EXTERN C_Func
                SECTION  .text:CODE:REORDER:NOROOT(2)
Asm_Func
                BL C_Func

                END