
#include <stdexcept>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include "../config.h"
#include "../lib.h"
#include "../arch/arch.h"
#include "os.h"
#include "os-lib.h"

namespace OS
{
	struct Process
	{
		uint16_t pc;
		uint16_t registers[8];
		bool running;
		uint16_t base;
		uint16_t limit;
	};

	static Process current_process;
	static Process idle_process;
	Arch::Cpu *cpu;
	static bool is_computer_running = false;

	void setup_process_memory(const Process &p)
	{
		cpu->set_vmem_mode(VmemMode::BaseLimit);
		cpu->set_vmem_paddr_base(p.base);
		cpu->set_vmem_size(p.limit);
	}

	std::vector<uint16_t> read_binary_file(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::binary | std::ios::ate);
		if (!file)
		{
			throw std::runtime_error("Failed to open file: " + filename);
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		size_t word_count = (size + 1) / 2;
		std::vector<uint16_t> buffer(word_count);

		if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
		{
			throw std::runtime_error("Failed to read file: " + filename);
		}
		return buffer;
	}

	void init_idle()
	{
		try
		{
			auto idle_code = read_binary_file("idle.bin");
			cpu->set_vmem_mode(VmemMode::Disabled);
			for (size_t i = 0; i < idle_code.size(); i++)
			{
				cpu->pmem_write(0x0000 + i, idle_code[i]);
				terminal_println(cpu, Terminal::Kernel,
								 "Carregado: " + std::to_string(idle_code[i]) +
									 " em " + std::to_string(0x0000 + i));
			}

			idle_process.pc = 0x0000;
			idle_process.running = true;
			idle_process.base = 0x0000;
			idle_process.limit = static_cast<uint16_t>(idle_code.size());
		}
		catch (const std::exception &e)
		{
			terminal_println(cpu, Terminal::Kernel, "Erro ao carregar idle.bin: " + std::string(e.what()));
			cpu->pmem_write(0x0000, 0x0000);
			idle_process.pc = 0x0000;
			idle_process.running = true;
			idle_process.base = 0x0000;
			idle_process.limit = 0x100;
		}
	}

	void load_program(const std::string &filename = "print.bin")
	{
		try
		{
			auto program_code = read_binary_file(filename);
			cpu->set_vmem_mode(VmemMode::Disabled);
			for (size_t i = 0; i < program_code.size(); i++)
			{
				cpu->pmem_write(0x2000 + i, program_code[i]);
			}
			current_process.pc = 0x2000;
			current_process.running = true;
			current_process.base = 0x2000;
			current_process.limit = static_cast<uint16_t>(program_code.size());
			for (int i = 0; i < 8; i++)
				current_process.registers[i] = 0;
			terminal_println(cpu, Terminal::Kernel, "Programa " + filename + " carregado em 0x2000");
		}
		catch (const std::exception &e)
		{
			terminal_println(cpu, Terminal::Kernel, "Erro ao carregar programa: " + std::string(e.what()));
			current_process.running = false;
		}
	}

	void kill_current_process()
	{
		current_process.running = false;
		terminal_println(cpu, Terminal::Kernel, "Processo encerrado");
	}
	// void handle_cpu_exception(CpuException ex)
	// {
	// 	if (ex == CpuException::VmemPageFault)
	// 	{
	// 		terminal_println(cpu, Terminal::Kernel, "Exceção: VmemPageFault em " + std::to_string(cpu->get_pc()));
	// 		kill_current_process();
	// 	}
	// }

	void boot(Arch::Cpu *cpu_ptr)
	{
		cpu = cpu_ptr;
		is_computer_running = true;
		terminal_println(cpu, Terminal::Kernel, "Saída do kernel aqui");
		init_idle();
		current_process = idle_process;

		terminal_println(cpu, Terminal::Command, "Comandos: q=sair, l=carregar, k=matar");
		terminal_println(cpu, Terminal::App, "Saida dos apps aqui");

		while (is_computer_running)
		{
			if (!current_process.running)
			{
				current_process = idle_process;
				terminal_println(cpu, Terminal::Kernel, "Executando idle process");
			}
			setup_process_memory(current_process);
			cpu->set_pc(current_process.pc);

			cpu->run_cycle();
			current_process.pc = cpu->get_pc();
			// tava aparecendo as coisas muito rapido, coloquei um timer aqui 
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}

	void interrupt(const Arch::InterruptCode interrupt)
	{
		uint16_t key = cpu->read_io(IO_Port::TerminalReadTypedChar);
		switch (key)
		{
		case 'q':
			terminal_println(cpu, Terminal::Kernel, "Desligando...");
			cpu->set_vmem_mode(VmemMode::Disabled);
			Arch::Computer::get().turn_off();
			is_computer_running = false;
			break;
		case 'l':
			if (!current_process.running || current_process.pc == idle_process.pc)
			{
				load_program();
			}
			break;
		case 'k':
			if (current_process.running && current_process.pc != idle_process.pc)
			{
				kill_current_process();
			}
			break;
		}
	}
	void syscall()
	{
		uint16_t code = cpu->get_gpr(0);
		switch (code)
		{
		case 0:
			current_process.running = false;
			break;
		case 1:
			terminal_print(cpu, Terminal::App, static_cast<char>(cpu->get_gpr(1)));
			break;
		case 2:
			terminal_print(cpu, Terminal::App, "\n");
			break;
		case 3:
			terminal_print(cpu, Terminal::App, std::to_string(cpu->get_gpr(1)));
			break;
		}
	}
} // namespace OS
