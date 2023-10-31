#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <ctype.h>

#define return_assert(cond) if (cond) return
#define return_assert_val(cond, val) if (cond) return val

#define BUFSZ 0x4000
#define TOKSZ 0x10
#define LBLSZ 0xFF

char *regs[] = {
	"a", "b", "c", "d", "e", "f", "t", "sp", "dp"
};

struct instruction {
	char name[4];
	int argc;
	int len;
	short opcode;
};

struct symbol {
	char name[4];
	int argc;
	int len;
	short opcode;
	short args[3];
	short def[LBLSZ];
};

struct label {
	int line;
	int addr;
	char name[LBLSZ];
};

struct var {
	int line;
	int addr;
	char name[LBLSZ];
};

struct instruction ins[] = {
	{ .name="lda", .argc=1, .len=2, .opcode=0x00 },
	{ .name="ldb", .argc=1, .len=2, .opcode=0x01 },
	{ .name="ldd", .argc=1, .len=2, .opcode=0x02 },
	{ .name="lde", .argc=1, .len=2, .opcode=0x03 },
	{ .name="lia", .argc=1, .len=2, .opcode=0x04 },
	{ .name="lib", .argc=1, .len=2, .opcode=0x05 },
	{ .name="lid", .argc=1, .len=2, .opcode=0x06 },
	{ .name="lie", .argc=1, .len=2, .opcode=0x07 },
	{ .name="add", .argc=0, .len=1, .opcode=0x08 },
	{ .name="sub", .argc=0, .len=1, .opcode=0x09 },
	{ .name="mul", .argc=0, .len=1, .opcode=0x0A },
	{ .name="div", .argc=0, .len=1, .opcode=0x0B },
	{ .name="xor", .argc=2, .len=3, .opcode=0x0C },
	{ .name="or", .argc=2, .len=3, .opcode=0x0D },
	{ .name="and", .argc=2, .len=3, .opcode=0x0E },
	{ .name="jmp", .argc=1, .len=2, .opcode=0x0F },
	{ .name="jz", .argc=1, .len=2, .opcode=0x10 },
	{ .name="jnz", .argc=1, .len=2, .opcode=0x11 },
	{ .name="jc", .argc=1, .len=2, .opcode=0x12 },
	{ .name="jnc", .argc=1, .len=2, .opcode=0x13 },
	{ .name="sb", .argc=1, .len=2, .opcode=0x14 },
	{ .name="gb", .argc=1, .len=2, .opcode=0x15 },
	{ .name="hlt", .argc=0, .len=1, .opcode=0x16 },
	{ .name="eb", .argc=0, .len=1, .opcode=0x17 },
	{ .name="db", .argc=0, .len=1, .opcode=0x18 },
	{ .name="def", .argc=1, .len=2, .opcode=0x19 },
	{ .name="sp", .argc=1, .len=2, .opcode=0x1A },
	{ .name="dp", .argc=1, .len=2, .opcode=0x1B },
	{ .name="inc", .argc=1, .len=2, .opcode=0x1C },
	{ .name="dec", .argc=1, .len=2, .opcode=0x1D },
	{ .name="%", .argc=0, .len=1, .opcode=0x1E }
};

struct label labels[LBLSZ] = {};
int labelsIndex = 0;
struct symbol symbols[BUFSZ] = {};
int symbolsIndex = 0;
struct var vars[BUFSZ] = {};
int varsIndex = 0;

char byteCode[BUFSZ] = {};

char *src;
char *old_src;
int line;
int mem_ptr;
int var_ptr;

inline void fetchSource(FILE *fd);
void firstPass(void);
void secondPass(void);
void assemble(void);

int main(int argc, char **argv) {
	argc--;
	argv++;
	
	FILE *fd = fopen(*argv, "r");
	assert(fd != NULL);
	
	assert((src = malloc(BUFSZ * sizeof(char))) != NULL);
	assert((old_src = malloc(BUFSZ * sizeof(char))) != NULL); // two copies of asm src code necessary for two passes
	
	line = 1;
	mem_ptr = 0; // managing where labels point to in memory
	var_ptr = 0x4001; // managing where variables point to in memory
	
	fetchSource(fd);
	
	fclose(fd);
	
	firstPass();
	
	for(int i = 0; i < varsIndex; i++) {
		printf("VARIABLE: %s\nADDRESS: %04x\n", vars[i].name, vars[i].addr);
	}
	
	secondPass();
	
	for(int i = 0; i < labelsIndex; i++) {
		printf("LABEL: %s\nADDRESS: %04x\n", labels[i].name, labels[i].addr);
	}
	
	for(int i = 0; i < symbolsIndex; i++) {
		if(strcmp(symbols[i].name,  "def") == 0) {
			printf("======== %s ========\n\tLength: %d\n\tArg Count: %d\n\tOpcode: %02x\n", symbols[i].name, symbols[i].len, symbols[i].argc, symbols[i].opcode);
			printf("\tDefined Bytes: ");
			
			for(int i = 0; i < LBLSZ; i++) {
				if(symbols[i].def[i] == 0)
					break;
				
				printf("%02x ", symbols[i].def[i]);
			}
			printf("\n");
		} else {
			printf("======== %s ========\n\tLength: %d\n\tArg Count: %d\n\tOpcode: %02x\n\tArg 1: %04x\n\tArg 2: %04x\n", symbols[i].name, symbols[i].len, symbols[i].argc, symbols[i].opcode, symbols[i].args[0], symbols[i].args[1]);
		}
		
		
	}
	
	// assemble symbols into byte code
	assemble();
	
	for(int i = 0; i < 100; i++) {
		if(i % 5 == 0)
			printf("\n");
		
		printf("(%d) %02x ", i, (unsigned)(byteCode[i]));
	}
	
	// save resulting bytecode into an h16 output file
	char *buf = calloc(strlen(*argv) + 1, sizeof(char));
	char *ext = strchr(*argv, '.');
	int index = (int)(ext - (*argv));
	for(int i = 0; i < index; i++) {
		buf[i] = (*argv)[i];
	}
	strcat(buf, ".h16");
	
	fd = fopen(buf, "w");
	fwrite(byteCode, 1, BUFSZ * sizeof(char), fd);
	fclose(fd);
	
	free(src);
	free(old_src);
	
	return 0;
}

void assemble(void) { // assemble parsed symbols into bytecode
	int index = 0;
	
	for(int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
		struct symbol *cur = &(symbols[i]);
		if(strcmp(cur->name, "") == 0)
			break;
		
		char arg0[2] = {};
		char arg1[2] = {};
		
		arg0[0] = (cur->args[0] & 0xff00) >> 8;
		arg0[1] = (cur->args[0] & 0x00ff);
		arg1[0] = (cur->args[1] & 0xff00) >> 8;
		arg1[1] = (cur->args[1] & 0x00ff);
		
		byteCode[index] = cur->opcode;
		if(cur->opcode != 0x19) {
			if(cur->argc == 2) {
				byteCode[index + 1] = arg0[0];
				byteCode[index + 2] = arg0[1];
				byteCode[index + 3] = arg1[0];
				byteCode[index + 4] = arg1[1];
				index += 4;
			} else if(cur->argc == 1) {
				byteCode[index + 1] = arg0[0];
				byteCode[index + 2] = arg0[1];
				index += 2;
			}
			index++;
		} else {
			for(int j = 0; j < LBLSZ; j++) {
				if((unsigned)(cur->def[j]) == 0x00)
					break;
				
				byteCode[index + 1] = cur->def[j];
				index += 1;
			}
			byteCode[index + 1] = (unsigned) 0xFF;
			index += 2;
		}
	}
}

void secondPass(void) { // parse arguments into bytecode and save into symbols array
	char *ln = strtok(old_src, "\n");
	int line = 0;
	
	while(ln != NULL) {
		if(strchr(ln, ':') != NULL) { // labels already cached, ignore them
			ln = strtok(NULL, "\n");
			continue;
		}
		
		struct symbol *cur = &(symbols[line]); // the current symbol being operated on

		if(strcmp(cur->name, "def") == 0) { // def already cached, ignore it
			line++;
			continue;
		}
		
		// scan argument(s) of symbol[line]
				
		int count = cur->argc;
		int curArg = 0;
		while(count > 0) {
			char *space = strchr(ln, ' ');
			if (space == NULL)
				break;
			
			int index = (int)(space - ln) + 1;
			ln += index;
			
			if(*ln == '%') {
				char buf[3] = {
					*(ln + 1), *(ln + 2) == ' ' ? 0 : *(ln + 2), 0
				};
				int regIndex = 0; // grab the numerical representation of the register to fetch from
				for(; regIndex < 9; regIndex++) {
					if(strcmp(buf, regs[regIndex]) == 0)
						break;
				}
				
				// state changer
				if(symbols[line - 1].opcode != 0x1E) { // guard against multiple state changes
					struct symbol stateChanger = {0};
					strcpy(stateChanger.name, "%");
					stateChanger.argc = 0;
					stateChanger.len = 1;
					stateChanger.args[0] = 0;
					stateChanger.args[1] = 0;
					stateChanger.opcode = 0x1E;
					
					// push the array back and place the state changer symbol
					for(int i = sizeof(symbols) / sizeof(symbols[0]); i > line; i--) {
						symbols[i] = symbols[i - 1];
					}
					
					symbols[line] = stateChanger;
					mem_ptr += 2;
					line++;
				}
				
				// place numerical register identifiers inside symbol arg list
				cur->args[curArg] = regIndex;
				curArg++;
			} else {
				// pull the number from the line
				char buf[6];
				for(int i = 0; i < 6; i++) {
					if(ln[i] == ' ' || ln[i] == '\t')
						break;
					
					buf[i] = ln[i];
				}
				
				if(isdigit(*buf)) {
					int arg = strtol(buf, NULL, 10);
					symbols[line].args[curArg] = arg;
					curArg++;
				} else {
					// labels or variables
					for(int i = 0; i < labelsIndex; i++) {
						if(strcmp(labels[i].name, buf) == 0) {
							symbols[line].args[0] = labels[i].addr;
							curArg++;
							break;
						}
					}
					
					for(int i = 0; i < varsIndex; i++) {
						if(strcmp(vars[i].name, buf) == 0) {
							symbols[line].args[0] = vars[i].addr;
							curArg++;
							break;
						}
					}
				}
			}
			
			count --;
		}
		
		printf("%s - %04x\n", symbols[line].name, symbols[line].args[0]);
		
		ln = strtok(NULL, "\n");
		line++;
	}
}

unsigned char checkForLabel(char *ln, char *lbl) {
	char *colon = strchr(ln, ':');
	int index = (int)(colon - ln);
	return_assert_val(index < 0, 0);
	
	char buf[index];
	for(int i = 0; i < index; i++) {
		buf[i] = ln[i];
	}
	buf[index] = 0x0;
	
	strcpy(lbl, buf);
	
	return 1;
}

unsigned char checkForVariable(char *ln, char *var) {
	char *colon = strchr(ln, ':');
	int index = (int)(colon - ln);
	return_assert_val(index < 0, 0);
	
	
	// check if the line has 'def' on the same line
	char ins[4] = {0};
	for(int i = index + 1; i < strlen(ln) + 1; i++) {
		ins[i - (index + 2)] = ln[i];
	}
	ins[3] = 0;
	
	if(strcmp(ins, "def") == 0) {
		char buf[LBLSZ];
		int i = 0;
		for(; i < LBLSZ; i++) {
			if(ln[i] == ':')
				break;
			
			buf[i] = ln[i];
		}
		buf[i] = 0x0;
		strcpy(var, buf);
		return 1;
	}
	
	return 0;
}

struct symbol getInstruction(char *ln, int *modMemPtr);

void firstPass(void) {
	char *ln = strtok(src, "\n");
	char *lbl = malloc(LBLSZ * sizeof(char));
	char *var = malloc(LBLSZ * sizeof(char));
	
	while(ln != NULL) {
		return_assert(*ln == ' ' || *ln == '\n' || *ln == '\t' || *ln == '\0' || *ln == 0xD || *ln == ',' || *ln == ';');
		
		memset(lbl, 0, LBLSZ * sizeof(char));
		
		// check for state changing character
		char *state = strchr(ln, '%');
		if(state != NULL) {
			mem_ptr += 1;
		}
		
		if(checkForVariable(ln, var)) {
			char *endOfDef = strchr(ln,  'f');
			int index = (int)(endOfDef - ln) + 2;
			
			char buf[strlen(ln) + 1];
			for(int i = index; i < strlen(ln) + 1; i++) {
				buf[i - index] = ln[i];
			}
			buf[strlen(buf)] = 0x0;
			
			struct var variable = {0}; // cache the variable information
			variable.line = line;
			variable.addr = var_ptr;
			strcpy(variable.name, var);
			
			vars[varsIndex] = variable;
			varsIndex++;
			
			struct symbol sym = {0}; // cache the variable's defined information
			strcpy(sym.name, "def");
			sym.argc = 1;
			sym.len = 2;
			sym.opcode = 0x19;
			mem_ptr += 2;
			
			if(*buf == '"') { // check for string based definition
				for(int i = 1; i < strlen(buf) + 1; i++) {
					if(buf[i] == '"')
						break;
					
					sym.def[i - 1] = (short) buf[i];
					var_ptr += 1;
					mem_ptr += 1;
				}
			} else { // not string based definition
				if(isdigit(*buf)) {
					short digit = (short) strtol(buf, NULL, 10);
					sym.def[0] = digit;
				} else {
					sym.def[0] = (short) (buf[1]);
				}
				
				var_ptr += 1;
				mem_ptr += 1;
			}
			
			symbols[symbolsIndex] = sym;
			symbolsIndex++;
		} else if(checkForLabel(ln, lbl) == 1) {
			struct label newLabel = {};
			newLabel.line = line;
			newLabel.addr = mem_ptr;
			
			printf("%d\n", mem_ptr);
			
			strcpy(newLabel.name, lbl);
			
			labels[labelsIndex] = newLabel;
			labelsIndex ++;
		} else { // 
			int modMemPtr = 0;
			struct symbol ret = getInstruction(ln, &modMemPtr);
			
			symbols[symbolsIndex] = ret;
			symbolsIndex ++;
			
			mem_ptr += modMemPtr;
		}
		
		ln = strtok(NULL, "\n");
		line ++;
	}
}

struct symbol getInstruction(char *ln, int *modMemPtr) { // return instruction data in the form of a struct
	struct symbol ret = {0};
	
	char buffer[4];
	char *space = strchr(ln, ' ');
	int insLen = (int)(space - ln);
	
	for(int i = 0; i < insLen; i++) {
		buffer[i] = ln[i];
	}
	buffer[insLen] = '\0';
	
	int numInstructions = (sizeof(ins) / sizeof(ins[0]));
	for(int i = 0; i < numInstructions; i++) {
		if(strcmp(buffer, ins[i].name) == 0) {
			char *stateChangeChar = strchr(ln, '%');
			if (stateChangeChar != NULL) {
				++(*modMemPtr);
			}
			
			(*modMemPtr) = ((ins[i].len - ins[i].argc) + (ins[i].argc * 2)); 
			/* ^ calculate mem_ptr modifier: (L - C) + (C * 2)
				L = Length of operation in bytes
				C = Arg count of the operation
			*/ 
			
			
			strcpy(ret.name, buffer);
			ret.argc = ins[i].argc;
			ret.len = ins[i].len;
			ret.opcode = ins[i].opcode;
			ret.args[0] = 0;
			ret.args[1] = 0; // cache return data
			
			break;
		}
	}
	
	return ret;
}

void fetchSource(FILE *fd) {
	size_t len = fread(src, sizeof(char), BUFSZ, fd);
	if(ferror(fd) != 0) {
		fputs("Error reading file", stderr);
	} else {
		src[len] = '\0';
		strcpy(old_src, src);
	}
}