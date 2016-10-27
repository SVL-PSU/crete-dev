#include <crete/custom_instr.h>

void send_dump_instr()
{
    crete_send_custom_instr_dump();
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    send_dump_instr();

	return 0;
}
