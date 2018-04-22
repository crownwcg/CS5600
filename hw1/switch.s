/*
 * file:        switch.s
 * description: assembly code to switch stack
 * class:       CS 5600, Spring 2018
 */

/*
 * switch_to - save stack pointer to *location_for_old_sp, set 
 *             stack pointer to 'new_value', and return.
 *             Note that the return takes place on the new stack.
 *
 * switch_to(int **location_for_old_sp, int *new_value)
 *   location_for_old_sp = *(SP+4) or *(BP+8)
 *   new_value = *(SP+8) or *(BP+12)
 *
 * For more details, see:
 *  http://pdos.csail.mit.edu/6.828/2004/lec/l2.html, GCC calling conventions
 */
 
switch_to:
        /* frame pointer - makes life easier in assembler, too 
	 */
	push %ebp 
	mov  %esp,%ebp
	
	/* debugging support - the last value pushed before switching
         * is a flag; check for that here and halt *before* switching
         * so you have a chance to debug
	 */ 
	mov  12(%ebp),%eax	/* new_value */
	cmpl $0xA5A5A5A5,(%eax)	/* flag value there? */
	je   ok			/* yes - skip */
	mov  $0,%eax
	mov  0(%eax),%eax	/* no - simple assert */
ok:	

	/* C calling conventions require that we preserve %ebp (already
         * saved above), %ebx, %esi, and %edi - push them onto the stack
	 */
	push %ebx
	push %esi
	push %edi

	push $0xA5A5A5A5	/* push the flag value */

	cmpl $0,8(%ebp)		/* is 'location_for_old' null? */
	je   skip
	mov  8(%ebp),%eax	/* no - save current stack pointer */
	mov  %esp,(%eax)	/* into 'location_for_old' */
skip:
	
	mov 12(%ebp),%esp	/* switch */

	pop  %eax		/* pop flag and ignore it */

	pop  %edi		/* pop callee-save registers */
	pop  %esi
	pop  %ebx

	pop  %ebp		/* and the frame pointer */

	ret			/* and return */
.global	switch_to
