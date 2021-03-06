


#include "zealot_predefine.h"


/*
注意，Fiber是无法达到这个效果的
int test_coroutine1(int argc, char * argv[])
{
    coroutine_t context;

    ZCE_OS::getcontext(&context);
    puts("Hello world");
    ZCE_OS::sleep(1);
    ZCE_OS::setcontext(&context);
    return 0;
}
*/


const unsigned int COROUTINE_LOOP_NUMBER = 3;

/* This is the iterator function. It is entered on the first call to
* swapcontext, and loops from 0 to 9. Each value is saved in i_from_iterator,
* and then swapcontext used to return to the main loop.  The main loop prints
* the value and calls swapcontext to swap back into the function. When the end
* of the loop is reached, the function exits, and execution switches to the
* context pointed to by main_context1. */
void loop(
    coroutine_t *loop_context,
    int *i_from_iterator)
{
    int i;

    for (i = 0; i < COROUTINE_LOOP_NUMBER; ++i)
    {
        /* Write the loop counter into the iterator return location. */
        *i_from_iterator = i;

        /* Save the loop context (this point in the code) into ''loop_context'',
        * and switch to other_context. */
        ZCE_OS::yeild_main(loop_context);
    }

    /* The function falls through to the calling context with an implicit
    * ''setcontext(&loop_context->uc_link);'' */
}


int test_coroutine2(int /*argc*/, char * /*argv*/[])
{

    /* The three contexts:
    *    (1) main_context1 : The point in main to which loop will return.
    *    (2) main_context2 : The point in main to which control from loop will
    *                        flow by switching contexts.
    *    (3) loop_context  : The point in loop to which control from main will
    *                        flow by switching contexts. */
    coroutine_t loop_context;

    

    /* Flag indicating that the iterator has completed. */
    volatile int iterator_finished;

    /* The iterator return value. */
    volatile int i_from_iterator =0;

    /* Fill in loop_context so that it makes swapcontext start loop. The
    * (void (*)(void)) typecast is to avoid a compiler warning but it is
    * not relevant to the behaviour of the function. */
    ZCE_OS::make_coroutine(&loop_context,
        8192*100,
        true,
        (ZCE_COROUTINE_3PARA)loop,
        (void *)&loop_context,
        (void *)&i_from_iterator,
        NULL);

    /* Clear the finished flag. */
    iterator_finished = 0;

    /* Save the current context into main_context1. When loop is finished,
    * control flow will return to this point. */
    //ZCE_OS::getcontext(&main_context1);

    if (!iterator_finished)
    {
        /* Set iterator_finished so that when the previous getcontext is
        * returned to via uc_link, the above if condition is false and the
        * iterator is not restarted. */
        iterator_finished = 1;

        while (COROUTINE_LOOP_NUMBER -1 > i_from_iterator)
        {
            /* Save this point into main_context2 and switch into the iterator.
            * The first call will begin loop.  Subsequent calls will switch to
            * the swapcontext in loop. */
            ZCE_OS::yeild_coroutine(&loop_context);
            printf("%d\n", i_from_iterator);
        }
    }

    return 0;

}
