#version 450
#extension GL_ARB_separate_shader_objects : enable


const int MASK=0xffff;


layout(early_fragment_tests) in;

layout(set=0, binding=3, r32i) uniform iimage2D ray_end_fragment_ids;

layout(location=0) in vec2 fragment_id_in;

void main()
{
	ivec2 fragment_id=ivec2(fragment_id_in);
	
	ivec2 current_frag_coord=ivec2(gl_FragCoord.xy);
	
	int begin_coord=fragment_id.x & (fragment_id.y<<16);
	int current_coord=current_frag_coord.x & (current_frag_coord.y<<16);
	
	int contained_coord=imageAtomicCompSwap(ray_end_fragment_ids, fragment_id, begin_coord, current_coord);
	int contained_coord_old=begin_coord;
	
	while(contained_coord!=contained_coord_old)
	{
		
		ivec2 decoded_contained_coord=ivec2(contained_coord & MASK, contained_coord>>16);
		
		if( distance(decoded_contained_coord, fragment_id)<distance(current_frag_coord, fragment_id))
			break;
		
		contained_coord_old=contained_coord;
		contained_coord=imageAtomicCompSwap(ray_end_fragment_ids, fragment_id, contained_coord_old, current_coord);
	}
}