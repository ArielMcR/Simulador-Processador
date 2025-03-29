#include <iostream>
#include <exception>

#include <cstdint>
#include <cstdlib>

#include <signal.h>

#include <my-lib/std.h>

#include "config.h"
#include "lib.h"
#include "arch/computer.h"
#include "arch/terminal.h"
#include "os/os.h"

// ---------------------------------------

void Lib::die ()
{
	endwin();
	Arch::Computer::get().get_terminal().dump(Arch::Terminal::Type::Kernel);
	std::exit(EXIT_FAILURE);
}

static void interrupt_handler (int dummy)
{
	mylib_throw_exception_msg("received interrupt signal");
}

int main (int argc, char **argv)
{
	signal(SIGINT, interrupt_handler);

	// ncurses start
	initscr();
	timeout(0); // non-blocking input
	noecho(); // don't print input

	try {
		Arch::Computer::init();
		OS::boot(&Arch::Computer::get().get_cpu());
		Arch::Computer::get().run();

		endwin();

		// print kernel msgs
		Arch::Computer::get().get_terminal().dump(Arch::Terminal::Type::Kernel);
		std::cout << std::endl;

		Arch::Computer::destroy();
	}
	catch (const std::exception& e) {
		endwin();
		std::cout << "Exception happenned!" << std::endl << e.what() << std::endl;
		Arch::Computer::get().get_terminal().dump(Arch::Terminal::Type::Kernel);
		return EXIT_FAILURE;
	}
	catch (...) {
		endwin();
		std::cout << "Unknown exception happenned!" << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}