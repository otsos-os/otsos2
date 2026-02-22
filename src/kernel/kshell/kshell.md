kshell - Kernel Shell
Simple shell for otsos that workd in ring 0
it starts in safe mode kernel and ctrl + shift + backspace for manually open kshell
commands:
    echo (commands/echo.c):
        echo <text> :: writes string
        echo <&math operation> :: evaluates math operation and writes result
        
        echo %<kvar> :: writes kernel variable:
            availables variables:
                %videoM :: what type of video mode is enabled (example output: "mesa, adress: ___", "vesa, adress: ___" "linear fb, adress: ___, multiboot<version (1 or 2)>" "vga" )
                %dkey :: what type of keyboard driver is enabled (writes c struct: static keyboard_driver_t ps2_driver )
                
                %prun :: prints all process that runs now (example output:
                    PID	NAME	STATE	

                    1	INIT	RUNNING	
                )

                %uname :: prints all uname info