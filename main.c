#include "gui.h"                 /* forward decl we’ll get from gui.c */
#include "utilities.h"
int main(int argc, char **argv)
{
    gui_init(&argc, &argv);      /* never returns until window closed  */
    return 0;
}