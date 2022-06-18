/* 046267 Computer Architecture - Winter 20/21 - HW #4 */

#include "core_api.h"
#include "sim_api.h"
#include <stdio.h>
#include <iostream>
#include <vector>
#include <queue>

using std::vector;
using std::priority_queue;

extern uint32_t prog_start; // the addr of the code block
extern uint32_t data_start; // the addr of the data block
extern Instruction** instructions; // where the instructions are kept
extern int32_t data[100]; // where the data is kept
extern uint32_t ticks; // the current clk tick
extern uint32_t read_tick; // the clk tick of the first attempt to read
extern uint32_t write_tick;// the clk tick for write
extern int load_store_latency[2];//load store
extern int switch_; //the cycles that switch between cycles takes
extern int threadnumber;

typedef enum {
	WAITING = 0, 
	READY = 1, 
	HALTED = 2
} THREAD_MODE;

class Thread {
public:
	int tid;
	int pc;
	int inst_num;
	int arrival_cyc;
	int cycles_remaining;
	vector<Instruction> insts;
	tcontext reg_file;
	THREAD_MODE mode;
	
	Thread(int id,int arrivalCyc, Instruction* inst, THREAD_MODE mode = READY);
	Thread() = default;
	Thread(const Thread& other);
	Thread& operator=(const Thread& other);
	~Thread() = default;

	void runInstruction(Instruction inst);

	bool operator>(const Thread& other) const;
	bool operator<(const Thread& other) const;
	bool operator==(const Thread& other) const;

};

class BlockedMT {
public:
	priority_queue<Thread> ready_PQ;
	priority_queue<Thread> waiting_PQ;
	priority_queue<Thread> halted_PQ;
	int inst_num;
	int cycles_count;

	BlockedMT();
	~BlockedMT() = default;


	void CORE_BlockedMT();
	void CORE_BlockedMT_CTX(tcontext* context, int threadid);
	double CORE_BlockedMT_CPI();

};

class FineGrainedMT {
public:
	vector<Thread> threads_vector;
	int inst_num;
	int cycles_count;
	vector<Thread>::iterator th_iterator;

	FineGrainedMT();
	~FineGrainedMT() = default;

	bool allHalted();
	bool isIdle();
	Thread& getNextReadyThread();
	void updateThreads();

	void CORE_FinegrainedMT();
	double CORE_FinegrainedMT_CPI();
	void CORE_FinegrainedMT_CTX(tcontext* context, int threadid);
};

BlockedMT* blocked_MT;
FineGrainedMT* fine_grained_MT;

void CORE_BlockedMT() {
	blocked_MT = new BlockedMT();
	blocked_MT->CORE_BlockedMT();
}

void CORE_FinegrainedMT() {
	fine_grained_MT = new FineGrainedMT();
	fine_grained_MT->CORE_FinegrainedMT();
}

double CORE_BlockedMT_CPI(){

	return blocked_MT->CORE_BlockedMT_CPI();
}

double CORE_FinegrainedMT_CPI(){
	return fine_grained_MT->CORE_FinegrainedMT_CPI();
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
	blocked_MT->CORE_BlockedMT_CTX(context, threadid);
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
	fine_grained_MT->CORE_FinegrainedMT_CTX(context, threadid);
}

Thread::Thread(int id, int arrivalCyc, Instruction* inst, THREAD_MODE mode) : tid(id), pc(0), inst_num(0), 
	arrival_cyc(arrivalCyc), cycles_remaining(0), mode(mode)
{
	for (int i = 0; inst[i].opcode != CMD_HALT; i++) {
		insts.push_back(inst[i]);
	}

	for (int i = 0; i < REGS_COUNT; i++) {
		reg_file.reg[i] = 0;
	}
}

void Thread::runInstruction(Instruction inst)
{
	cmd_opcode opc = inst.opcode;
	int off_imm = inst.isSrc2Imm ? inst.src2_index_imm : reg_file.reg[inst.src2_index_imm];
	inst_num++;

	switch (opc) {
	case CMD_NOP: // NOP
		break;
	case CMD_ADD:
	case CMD_ADDI:
		reg_file.reg[inst.dst_index] = reg_file.reg[inst.src1_index] + off_imm;
		break;
	case CMD_SUB:
	case CMD_SUBI:
		reg_file.reg[inst.dst_index] = reg_file.reg[inst.src1_index] - off_imm;
		break;
	case CMD_LOAD:
		SIM_MemDataRead(reg_file.reg[inst.src1_index] + off_imm, reg_file.reg + inst.dst_index);
		break;
	case CMD_STORE:
		SIM_MemDataWrite(reg_file.reg[inst.dst_index] + off_imm, reg_file.reg[inst.src1_index]);
		break;
	case CMD_HALT:
		break;
	}
}

bool Thread::operator>(const Thread& other) const
{
	if ((arrival_cyc == 0 && other.arrival_cyc == 0) || (arrival_cyc == other.arrival_cyc))
		return tid < other.tid;
	return arrival_cyc > other.arrival_cyc;
}

bool Thread::operator<(const Thread& other) const
{
	return !(*this > other);
}

bool Thread::operator==(const Thread& other) const
{
	return (!(*this > other) && !(*this < other));
}

BlockedMT::BlockedMT() : inst_num(0), cycles_count(0)
{
	for (int i = 0; i < threadnumber; i++) {
		Thread thread(i, 0, instructions[i]);
		ready_PQ.push(thread);
	}
}

//run the block simulator until all threads get halted
void BlockedMT::CORE_BlockedMT()
{
	

}

FineGrainedMT::FineGrainedMT()
{
	for (int i = 0; i < threadnumber; i++) {
		Thread thread(i, 0, instructions[i]);
		threads_vector.push_back(thread);
	}
}

bool FineGrainedMT::allHalted()
{
	vector<Thread>::iterator thread_it = threads_vector.begin();
	for (int i = 0; i < inst_num; i++) {
		if (thread_it->mode == HALTED) return false;
	}
	return true;
}

bool FineGrainedMT::isIdle()
{
	vector<Thread>::iterator thread_it = threads_vector.begin();
	for (int i = 0; i < threadnumber; i++) {
		if (thread_it->mode == READY) return false;
	}
	return true;
}

Thread& FineGrainedMT::getNextReadyThread()
{
	vector<Thread>::iterator it = th_iterator;
	while (it->mode != READY) {
		it++;
	}
	th_iterator = it;
	return *it;
}

void FineGrainedMT::updateThreads()
{
	for (int i = 0; i < threadnumber; i++) {
		if (threads_vector[i].mode == WAITING) {
			threads_vector[i].cycles_remaining--;
			if (threads_vector[i].cycles_remaining == 0) {
				threads_vector[i].pc++;
				threads_vector[i].mode = READY;
			}
		}
	}
}

//run simulator of fine-grained MT until all threads get halted
void FineGrainedMT::CORE_FinegrainedMT()
{
	Thread runningTH;// = threads_vector.front();

	while (!allHalted()) {
		cycles_count++;
		if (!isIdle()) { //there are ready threads
			runningTH = getNextReadyThread();
			int pc = runningTH.pc;
			runningTH.runInstruction(runningTH.insts[pc]); //run command
			updateThreads();

			int op = runningTH.insts[pc].opcode;
			if (op == CMD_LOAD || op == CMD_STORE) {
				runningTH.cycles_remaining = load_store_latency[op];
				runningTH.mode = WAITING;
			}
			else if (op == CMD_HALT) {
				runningTH.mode = HALTED;
			}
			runningTH.pc++;
		}
		else {
			updateThreads();
		}
	}
}
