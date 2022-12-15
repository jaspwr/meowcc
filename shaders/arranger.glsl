#version 460 core

#define CODEGEN_END 0
#define DEFAULT_CONDITION 1
#define CONDITIAL_EQAULS_TO 2
#define END_CONDITION 3
#define END_INSTRUCTION 4
#define INTERNAL_ELEMENT 5
#define REFERENCE_IDENTIFIER 6
#define REFERENCE_IMPLICIT_TYPE_IDENTIFIER 7
#define INSERSION_IDENTIFIER 8
#define INSERSION_IMPLICIT_TYPE_IDENTIFIER 9
#define SELF_REGISTER_IDENTIFIER 10
#define SELF_REGISTER_VALUE 11
#define LANGUAGE_TOKEN_IDENTIFIER 12


layout(local_size_x=32,local_size_y=1,local_size_z=1)in;
layout(binding = 3) uniform atomic_uint ast_total_count;

struct ast_node
{
	int node_token;
	int depth;
	int volume;
	int operand_count;
	int operands[4];
	int final_position;
};

struct organised_node
{
	int node_token;
	int own_organised_location;
	int operand_count;
	int operands[4];
};

layout(std430,binding=6)coherent buffer ast
{
	ast_node ast_data[];
};

layout(std430,binding=4)writeonly coherent buffer organised_tree
{
	organised_node organised_tree_data[];
};

void main(){
	barrier();
	memoryBarrier();

	//ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
	//ivec2 dims = ivec2(500);
	
	bool found = false;
	int _pos = int(gl_GlobalInvocationID.x);
	uint entry_node = atomicCounter(ast_total_count) + 20;


	uint biggest_volume = 0;
	uint biggest_volume_index = entry_node;
	while(entry_node > 0) {
		if (ast_data[entry_node].volume+1 > biggest_volume) {
			biggest_volume = ast_data[entry_node].volume+1;
			biggest_volume_index = entry_node;
		}
		entry_node--;
	}
	entry_node = biggest_volume_index;


	int working_node = int(entry_node);
	int oper = 0;
	int oper_count = ast_data[working_node].operand_count;
	int working_volume=_pos;

	if (_pos >= biggest_volume) {
		organised_tree_data[_pos].node_token = 0;
		memoryBarrier();
		barrier();
		return;
	}

	int bottom = 0;
	int top = ast_data[working_node].volume+1;
	
	int debug_bail = 0;
	barrier();
	memoryBarrier();
	while(debug_bail < 400){
		barrier();
		memoryBarrier();

		if(working_volume < 0 || oper > oper_count)
			break;

		if (working_volume == top -1) {
			//found
			found = true;
			working_node = ast_data[working_node].operands[oper];
			break;
		}
		if (working_volume < top) {
			// moves down a branch
			working_node = ast_data[working_node].operands[oper];
			oper_count = ast_data[working_node].operand_count;
			//int cdgn = codegen_data[ast_data[working_node].node_token];

			working_volume -= bottom; //+ (cdgn != 0 ? codegen_data[cdgn] - 1 : 0);
			oper = 0;
			bottom = 0;
			//top = ast_data[ast_data[working_node].operands[0]].volume;
			top = ast_data[ast_data[working_node].operands[oper]].volume+1;
		} else {
			bottom = top;
			oper++;
			top += ast_data[ast_data[working_node].operands[oper]].volume+1;
		}
		debug_bail++;
	}
	barrier();
	memoryBarrier();


	if (found) {
		organised_tree_data[_pos].node_token = ast_data[working_node].node_token;
		organised_tree_data[_pos].own_organised_location = working_node;
		memoryBarrierBuffer();
		barrier();
		ast_data[working_node].final_position = _pos; // Works
		memoryBarrierBuffer();
		barrier();
	} else {
		organised_tree_data[_pos].node_token=0;
		memoryBarrierBuffer();
		barrier();
	}
	memoryBarrierBuffer();
	barrier();

}