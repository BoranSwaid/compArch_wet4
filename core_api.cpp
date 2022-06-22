/* 046267 Computer Architecture - Winter 20/21 - HW #4 */

#include "core_api.h"
#include "sim_api.h"
#include <stdio.h>
#include <iostream>

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

class BlockedMT;
class FinerGrainedMT;

BlockedMT* blocked;
FinerGrainedMT* finer_grained;

typedef enum {
	idle = 0,
	running = 1,
	halted = 2 
}State;

typedef struct thread_t {
	int inst_offset;
	int clocks_num;
	int lat_remain;
	State state;
	tcontext reg_file;
} Thread;

class BlockedMT {
public:
	int instructions;
	int cycles;
	int num_of_threads;
	int store_latency;
	int load_latency;
	int cycles_per_switch;

	Thread** threads;
	bool switchFlag;

	BlockedMT() : instructions(0), cycles(0) {
		this->num_of_threads = SIM_GetThreadsNum();
		this->store_latency = SIM_GetStoreLat();
		this->load_latency = SIM_GetLoadLat();
		this->cycles_per_switch = SIM_GetSwitchCycles();
		this->switchFlag = false;

		// create the threads and initalize them
		threads = (Thread**)malloc(sizeof(Thread*) * num_of_threads);
		for (int i = 0; i < num_of_threads; i++) {
			threads[i] = (Thread*)malloc(sizeof(Thread));
			threads[i]->inst_offset = 0;
			threads[i]->lat_remain = 0;
			threads[i]->clocks_num = 0;
			threads[i]->state = idle;

			// initialize registers
			for (int j = 0; j < REGS_COUNT; j++)
				threads[i]->reg_file.reg[j] = 0;

		}
	}
	~BlockedMT() {
		for (int i = 0; i < num_of_threads; i++)
		{
			free(threads[i]);
		}
		free(threads);
	}

	void runInstruction(Instruction currInst, int current_thread) {
		switch (currInst.opcode) {
		case CMD_NOP:
			//do nothing
			break;

		case CMD_ADD: {
			//update registers
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] +
				threads[current_thread]->reg_file.reg[currInst.src2_index_imm];
			break;
		}

		case CMD_SUB: {
			//update registers			
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] -
				threads[current_thread]->reg_file.reg[currInst.src2_index_imm];
			break;
		}

		case CMD_ADDI: {
			//update registers
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] +
				currInst.src2_index_imm;
			break;
		}

		case CMD_SUBI: {
			//update registers
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] -
				currInst.src2_index_imm;
			break;
		}

		case CMD_LOAD: {
			//put the current_thread in idle state and set lat_remain
			threads[current_thread]->state = idle;
			threads[current_thread]->lat_remain = load_latency;

			//prepare arguments for reading
			int32_t* dst = &(threads[current_thread]->reg_file.reg[currInst.dst_index]);
			uint32_t memory_address = threads[current_thread]->reg_file.reg[currInst.src1_index];
			if (currInst.isSrc2Imm) memory_address += currInst.src2_index_imm;
			else memory_address += threads[current_thread]->reg_file.reg[currInst.src2_index_imm];

			SIM_MemDataRead(memory_address, dst);
			switchFlag = true;
			break;
		}
		case CMD_STORE: {
			//put the current_thread in idle state and set lat_remain
			threads[current_thread]->state = idle;
			threads[current_thread]->lat_remain = store_latency;

			//prepare arguments to read
			int32_t val = threads[current_thread]->reg_file.reg[currInst.src1_index];
			uint32_t memory_address = threads[current_thread]->reg_file.reg[currInst.dst_index];
			if (currInst.isSrc2Imm) memory_address += currInst.src2_index_imm;
			else memory_address += threads[current_thread]->reg_file.reg[currInst.src2_index_imm];

			SIM_MemDataWrite(memory_address, val);
			switchFlag = true;
			break;
		}
		case CMD_HALT: {
			threads[current_thread]->state = halted;
			switchFlag = true;

			break;
			}
		}
	}

	void CORE_BlockedMT();
	double CORE_BlockedMT_CPI();
	void CORE_BlockedMT_CTX(tcontext* context, int threadid);

};

class FinerGrainedMT {
public:
	int instructions;
	int cycles;
	int num_of_threads;
	int store_latency;
	int load_latency;
	int cycles_per_switch;

	Thread** threads;
	bool switchFlag;

	FinerGrainedMT() : instructions(0), cycles(0) {
		this->num_of_threads = SIM_GetThreadsNum();
		this->store_latency = SIM_GetStoreLat();
		this->load_latency = SIM_GetLoadLat();
		this->cycles_per_switch = SIM_GetSwitchCycles();
		this->switchFlag = false;

		// create the threads and initialize them
		threads = (Thread**)malloc(sizeof(Thread*) * num_of_threads);
		for (int i = 0; i < num_of_threads; i++) {
			threads[i] = (Thread*)malloc(sizeof(Thread));
			threads[i]->inst_offset = 0;
			threads[i]->lat_remain = 0;
			threads[i]->clocks_num = 0;
			threads[i]->state = idle;

			// initialize register file
			for (int j = 0; j < REGS_COUNT; j++)
				threads[i]->reg_file.reg[j] = 0;

		}
	}

	~FinerGrainedMT() {
		for (int i = 0; i < num_of_threads; i++)
		{
			free(threads[i]);
		}
		free(threads);
	}

	void runInstruction(Instruction currInst, int current_thread) {
		switch (currInst.opcode) {

		case CMD_NOP:
			//do nothing
			break;

		case CMD_ADD: {
			//update registers
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] +
				threads[current_thread]->reg_file.reg[currInst.src2_index_imm];
			break;
		}

		case CMD_SUB:{
			//update registers				
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] -
				threads[current_thread]->reg_file.reg[currInst.src2_index_imm];
			break;
			}
		case CMD_ADDI:{
			//update registers
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] +
				currInst.src2_index_imm;
			break;
		}
		case CMD_SUBI:{
			//update registers
			threads[current_thread]->reg_file.reg[currInst.dst_index] = threads[current_thread]->reg_file.reg[currInst.src1_index] -
				currInst.src2_index_imm;
			break;
		}

		case CMD_LOAD: {
			//put the current_thread in idle and set lat_remain
			threads[current_thread]->state = idle;
			threads[current_thread]->lat_remain = load_latency;

			//prepare arguments to read
			int32_t* dst = &(threads[current_thread]->reg_file.reg[currInst.dst_index]);
			uint32_t memory_address = threads[current_thread]->reg_file.reg[currInst.src1_index];
			if (currInst.isSrc2Imm) memory_address += currInst.src2_index_imm;
			else memory_address += threads[current_thread]->reg_file.reg[currInst.src2_index_imm];

			SIM_MemDataRead(memory_address, dst);

			break;
		}
		case CMD_STORE: {
			//put the current_thread in idle and set lat_remain
			threads[current_thread]->state = idle;
			threads[current_thread]->lat_remain = store_latency;

			//prepare arguments to read
			int32_t val = threads[current_thread]->reg_file.reg[currInst.src1_index];
			uint32_t memory_address = threads[current_thread]->reg_file.reg[currInst.dst_index];
			if (currInst.isSrc2Imm) memory_address += currInst.src2_index_imm;
			else memory_address += threads[current_thread]->reg_file.reg[currInst.src2_index_imm];

			SIM_MemDataWrite(memory_address, val);

			break;
		}
		case CMD_HALT: {
			threads[current_thread]->state = halted;
			break;
		}
		}
	}

	void CORE_FinegrainedMT();
	double CORE_FinegrainedMT_CPI();
	void CORE_FinegrainedMT_CTX(tcontext* context, int threadid);

};

// static functions
static bool finished(Thread **threads);
static bool thread_available(Thread *current_thread);
static void update_latency(Thread **threads , int current_thread);

void BlockedMT::CORE_BlockedMT() {
	int current_thread = 0;

	while(!finished(threads)){
		 
		//check if can execute current_thread, otherwise switch
		if(!thread_available(threads[current_thread]) || switchFlag){

			/* check if we can exucute diffrent thread*/
			int i = (current_thread + 1) % num_of_threads;
			while(i != current_thread){
				if(thread_available(threads[i]))
					break;
				i = (i + 1) % num_of_threads;
			}
			if( i == current_thread){
				// nothing to be executed so we wait for one cycle and update latencies to wait */
				
				update_latency(threads , num_of_threads);
				threads[current_thread]->clocks_num++;
  				this->cycles++;

				if(threads[current_thread]->lat_remain==0)
				{
					switchFlag=false;
				}
				continue;
			}
			else {
				current_thread = i;
				this->cycles += cycles_per_switch;
				for(int j = 0; j < cycles_per_switch; j++)
				{
					update_latency(threads, current_thread);
				}
			}
			switchFlag=false;
		}
		
		Instruction currInst;

		SIM_MemInstRead(threads[current_thread]->inst_offset, &currInst, current_thread);
	
		runInstruction(currInst, current_thread);
		update_latency(threads , current_thread);
		threads[current_thread]->inst_offset+=1;
		threads[current_thread]->clocks_num++;
		this->cycles++;
		this->instructions++;
	}
	return;
}

void FinerGrainedMT::CORE_FinegrainedMT() {
		
	int current_thread = 0;
	
	while(!finished(threads)){
		
		/* check if current_thread can be executed, otherwise switch*/
		if(!thread_available(threads[current_thread])){
			
			/* check if we can exucute diffrent thread*/
			int i = (current_thread+1) % num_of_threads;
			while(i != current_thread){
				if(thread_available(threads[i]))
					break;
				i = (i + 1) % num_of_threads;
			}
			if( i == current_thread){
				//nothing to be executed, wait for one cycle and update latencies
				update_latency(threads , num_of_threads);

				threads[current_thread]->clocks_num++;
				this->cycles++;
				continue;
			}
			else current_thread = i;
		}
		
		Instruction currInst;
		SIM_MemInstRead(threads[current_thread]->inst_offset, &currInst, current_thread);
		
		runInstruction(currInst, current_thread);
		
		update_latency(threads , current_thread);
		threads[current_thread]->inst_offset += 1;
		threads[current_thread]->clocks_num++;
		this->instructions++;
		this->cycles++;
		
		current_thread = (current_thread + 1) % num_of_threads;
	}
	
	return;
}

double BlockedMT::CORE_BlockedMT_CPI(){
	return (double)(this->cycles) / (double)(this->instructions);
}

double FinerGrainedMT::CORE_FinegrainedMT_CPI(){
	return (double)(this->cycles) / (double)(this->instructions) ;
}

void BlockedMT::CORE_BlockedMT_CTX(tcontext* context, int threadid) {
	for(int i = 0; i < REGS_COUNT; i++){
		context->reg[threadid * REGS_COUNT + i] = threads[threadid]->reg_file.reg[i];
	}
}

void FinerGrainedMT::CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
	for(int i = 0; i < REGS_COUNT; i++){
		context->reg[threadid * REGS_COUNT + i] = threads[threadid]->reg_file.reg[i];
	}
}

//check if there is ready threads
static bool finished(Thread **threads){
		int num_of_threads = SIM_GetThreadsNum();
		for(int i = 0; i < num_of_threads; i++){	
			if(threads[i]->state != halted)
				return false;
		}
		return true;
}

// check if can execute current_thread
static bool thread_available(Thread *current_thread){
		if(current_thread->state==halted){
			return false;
		}
		if(current_thread->state==idle){
			//still waiting for memory 
			if(current_thread->lat_remain !=0){
				return false;
			}
			else{
				current_thread->state = running;
				return true;
			}
		}
		return true;		
}

//update all threads lat_remain
static void update_latency(Thread **threads , int current_thread){
	int num_of_threads = SIM_GetThreadsNum();

	for(int i = 0; i < num_of_threads; i++){
		//update all threads but current_thread
		if(i != current_thread){
			if(threads[i]->state == idle){
				int latency = threads[i]->lat_remain;
				if(latency > 0){
					latency--;
					if(latency == 0){
						threads[i]->lat_remain = 0;
						threads[i]->state = running;
					}
					else threads[i]->lat_remain--;	
				}	
			}
		}
	}
}

/*------------------------------------------------main fubctions------------------------------------------------------------*/
void CORE_BlockedMT() {
	blocked = new BlockedMT();
	blocked->CORE_BlockedMT();
}

void CORE_FinegrainedMT() {
	finer_grained = new FinerGrainedMT();
	finer_grained->CORE_FinegrainedMT();
}

double CORE_BlockedMT_CPI() {
	return blocked->CORE_BlockedMT_CPI();
}

double CORE_FinegrainedMT_CPI() {
	return finer_grained->CORE_FinegrainedMT_CPI();
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
	blocked->CORE_BlockedMT_CTX(context, threadid);
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
	finer_grained->CORE_FinegrainedMT_CTX(context, threadid);
}


