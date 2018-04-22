/*
 * setup_stack(stack, function, arg1, arg2) - sets up a stack so that
 * switching to it from 'do_switch' will call 'function' with arguments
 * 'arg1' and 'arg2'. Returns the resulting stack pointer.
 *
 * works fine with functions that take one argument ('arg1') or no
 * arguments, as well - just pass zero for the unused arguments.
 */
void *setup_stack(int *stack, void *func, void *arg1, void *arg2)
{
    int old_bp = (int)stack;	/* top frame - SP = BP */

    *(--stack) = 0x3A3A3A3A;    /* guard zone */
    *(--stack) = 0x3A3A3A3A;
    *(--stack) = 0x3A3A3A3A;

    /* this is the stack frame "calling" 'func'
     */
    *(--stack) = (int)arg2;     /* argument */
    *(--stack) = (int)arg1;     /* argument (reverse order) */
    *(--stack) = 0;             /* fake return address (to 'func') */

    /* this is the stack frame calling 'switch_to'
     */
    *(--stack) = (int)func;     /* return address */
    *(--stack) = old_bp;        /* %ebp */
    *(--stack) = 0;             /* %ebx */
    *(--stack) = 0;             /* %esi */
    *(--stack) = 0;             /* %edi */
    *(--stack) = 0xa5a5a5a5;    /* valid stack flag */

    return stack;
}
