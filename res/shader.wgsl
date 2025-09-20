// Contains all of our uniforms

// Ensure that EACH variable in the uniform starts at a multiple of 16 bytes
struct MyUniforms { // Total size of the struct has to be a multiple of the alignment size of its largest field
	color: vec4f,
	time: f32,
};

@group(0) @binding(0) var<uniform> u_myUniforms: MyUniforms;

// The struct passed to the vertex assembler stage
struct VertexInput {
	@location(0) position: vec2f,
	@location(1) color: vec3f,
};

// Cannot directly send struct to fragment through C++, must return it from vertex shader
struct VertexOutput {
	@builtin(position) position: vec4f, // @builtin(position) is required by the rasterizer
	@location(0) color: vec3f,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	let ratio = 1920.0 / 1080.0; // Target aspect ratio of surface
	
	var offset = vec2f(-0.6875, -0.463); // The offset that we want to apply
	offset += 0.3 * vec2f(cos(u_myUniforms.time), sin(u_myUniforms.time));
	
	out.position = vec4f(in.position.x + offset.x, (in.position.y + offset.y) * ratio, 0.0, 1.0); // Same as before
	out.color = in.color; // Forward to FS
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let color = in.color * u_myUniforms.color.rgb;
	// Gamma correction (Not needed)
	// let linear_color = pow(color, vec3f(2.2));
	return vec4f(color, 1.0);
}