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
using namespace Arch;

namespace OS
{
	struct Process
	{
		uint16_t pc;
		uint16_t registers[8];
		bool running;
		uint16_t base;
		uint16_t limit;
		
		void reset_registers() {
			for (int i = 0; i < 8; i++)
				registers[i] = 0;
		}
		
		bool is_idle() const {
			return pc == 0x0000;
		}
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
	void load_program(const std::string &filename = "perfect-squares.bin")
	{
		try
		{
			auto program_code = Lib::load_from_disk_to_16bit_buffer(filename);
			cpu->set_vmem_mode(VmemMode::Disabled);
			terminal_println(cpu, Terminal::Kernel, "program_code size = " + std::to_string(program_code.size()));
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
	
	void init_idle()
	{
		try
		{
			auto idle_code = Lib::load_from_disk_to_16bit_buffer("idle.bin");
			terminal_println(cpu, Terminal::Kernel, "idle_code size = " + std::to_string(idle_code.size()));

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
	


	void load_process(const std::string &filename, Process &process, uint16_t base)
	{
		try
		{
			auto program_code = Lib::load_from_disk_to_16bit_buffer(filename);
			cpu->set_vmem_mode(VmemMode::Disabled);
			for (size_t i = 0; i < program_code.size(); i++)
			{
				cpu->pmem_write(base + i, program_code[i]);
			}
			process.pc = base;
			process.running = true;
			process.base = base;
			process.limit = static_cast<uint16_t>(program_code.size());
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

	void handle_cpu_exception(const CpuException &ex)
	{
		switch (ex.type) {
		case CpuException::Type::VmemPageFault:
		case CpuException::Type::VmemGPFnotReadable:
		case CpuException::Type::VmemGPFnotWritable:
		case CpuException::Type::VmemGPFnotExecutable:
			terminal_println(cpu, Terminal::Kernel, "GPF em " + std::to_string(ex.vaddr));
			kill_current_process();
			break;
		case CpuException::Type::GPFinvalidInstruction:
			terminal_println(cpu, Terminal::Kernel, "Instrução inválida em " + std::to_string(ex.vaddr));
			kill_current_process();
			break;
		}
	}

	void boot(Arch::Cpu *cpu_ptr)
	{
		if (!cpu_ptr) {
			throw std::runtime_error("CPU não pode ser nula");
		}
		
		cpu = cpu_ptr;
		is_computer_running = true;
		
		try {
			terminal_println(cpu, Terminal::Kernel, "Iniciando kernel...");
			init_idle();
			current_process = idle_process;
			
			terminal_println(cpu, Terminal::Command, "Comandos: q=sair, l=carregar, k=matar");
			terminal_println(cpu, Terminal::App, "Saida dos apps aqui");
			
			setup_process_memory(current_process);
			cpu->set_pc(current_process.pc);
			
		}
		catch (const std::exception &e) {
			terminal_println(cpu, Terminal::Kernel, "Erro no boot: " + std::string(e.what()));
			is_computer_running = false;
		}
	}
	

	void interrupt(const Arch::InterruptCode interrupt)
	{
		uint16_t key = cpu->read_io(Arch::IO_Port::TerminalReadTypedChar);
		switch (key)
		{
		case 'q':
			terminal_println(cpu, Terminal::Kernel, "Desligando...");
			cpu->set_vmem_mode(VmemMode::Disabled);
			Arch::Computer::get().turn_off();
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
			cpu->force_interrupt(Arch::InterruptCode::CpuException);
			terminal_println(cpu, Terminal::Kernel, "Programa finalizado");
			
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
		default:
			terminal_println(cpu, Terminal::Kernel, "Syscall não suportada: " + std::to_string(code));
			break;
		}
	}
} // namespace OS
