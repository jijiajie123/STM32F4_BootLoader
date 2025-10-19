
                AREA    |.text|, CODE, READONLY

; Reset handler
jump_to_app     PROC
                EXPORT  jump_to_app

                ;set vtor
                LDR R1, =0xE000ED08
                STR R0, [R1]
                LDR sp, [R0, #0]
                LDR pc, [R0, #4]
                
                ENDP
