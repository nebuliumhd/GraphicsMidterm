// Contains all of our uniforms

// Ensure that EACH variable in the uniform starts at a multiple of 16 bytes
struct MyUniforms { // Total size of the struct has to be a multiple of the alignment size of its largest field
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	modelMatrix: mat4x4f,
	color: vec4f,
	time: f32,
};

struct LightingUniforms {
	directions: array<vec4f, 2>,
	colors: array<vec4f, 2>,
}

const pi = 3.14159265359;
@group(0) @binding(0) var<uniform> u_myUniforms: MyUniforms;
@group(0) @binding(1) var u_baseColorTexture: texture_2d<f32>;
@group(0) @binding(2) var u_textureSampler: sampler;
@group(0) @binding(3) var<uniform> u_lighting: LightingUniforms;

// The struct passed to the vertex assembler stage
struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

// Cannot directly send struct to fragment through C++, must return it from vertex shader
struct VertexOutput {
	@builtin(position) position: vec4f, // @builtin(position) is required by the rasterizer
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

@vertex
fn vs_main(v_in: VertexInput) -> VertexOutput {
	var v_out: VertexOutput;
	v_out.position =
		u_myUniforms.projectionMatrix *
		u_myUniforms.viewMatrix *
		u_myUniforms.modelMatrix *
		vec4f(
			v_in.position,
			1.0
		);
	v_out.normal = (u_myUniforms.modelMatrix * vec4f(v_in.normal, 0.0)).xyz;
	v_out.color = v_in.color;
	v_out.uv = v_in.uv;
	return v_out;
}

@fragment
fn fs_main(f_in: VertexOutput) -> @location(0) vec4f {
	let normal = normalize(f_in.normal);
	var shading = vec3f(0.0);
	for (var i: i32 = 0; i < 2; i++) {
		let direction = normalize(u_lighting.directions[i].xyz);
		let color = u_lighting.colors[i].rgb;
		shading += max(0.0, dot(direction, normal)) * color;
	}

	// Sample texture
	let baseColor = textureSample(u_baseColorTexture, u_textureSampler, f_in.uv).rgb;
	let color = baseColor * shading;

	// Gamma correction (Not needed)
	// let linear_color = pow(color, vec3f(2.2));
	return vec4f(color, u_myUniforms.color.a);
}