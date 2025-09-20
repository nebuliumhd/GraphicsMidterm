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
	let offset = vec2f(-0.6875, -0.463); // The offset that we want to apply
	out.position = vec4f(in.position.x + offset.x, (in.position.y + offset.y) * ratio, 0.0, 1.0); // Same as before
	out.color = in.color; // Forward to FS
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	// Gamma correction (Not needed)
	// let linear_color = pow(in.color, vec3f(2.2));
	return vec4f(in.color, 1.0);
}